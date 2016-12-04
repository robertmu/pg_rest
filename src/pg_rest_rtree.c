/* -------------------------------------------------------------------------
 *
 * pq_rest_rtree.c
 *   url match routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_rtree.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef enum {
    PGREST_RTREE_NODE_STATIC = 0,
    PGREST_RTREE_NODE_ROOT,
    PGREST_RTREE_NODE_PARAM,
    PGREST_RTREE_NODE_CATCHALL
} pgrest_rtree_nodetype_e;

struct pgrest_rtree_node_s {
    pgrest_string_t           path;
    bool                      wild_child;
    pgrest_rtree_nodetype_e   type;
    int                       max_params;
    pgrest_string_t           indices;
    pgrest_rtree_node_t     **children;
    size_t                    nchildren;
    size_t                    capacity;
    void                     *handler;
    int                       priority;
};

#define INIT_STRING(dst, src, size)                                        \
do {                                                                       \
    (dst).base = (src);                                                    \
    (dst).len = (size);                                                    \
} while (0)

#define COPY_STRING(dst, src, size)                                        \
do {                                                                       \
    if ((dst).base) {                                                      \
        pfree((dst).base);                                                 \
    }                                                                      \
    (dst).base = palloc((size) + 1);                                       \
    memcpy((dst).base, (src), (size));                                     \
    (dst).base[(size)] = '\0';                                             \
    (dst).len = (size);                                                    \
} while (0)

#define APPEND_STRING(dst, src, size)                                      \
do {                                                                       \
    size_t n = (dst).len + (size);                                         \
                                                                           \
    (dst).base = realloc((dst).base, n + 1);                               \
    memcpy((dst).base + (dst).len, src, size);                             \
    (dst).base[n] = '\0';                                                  \
    (dst).len = n;                                                         \
} while (0)

#define INIT_NODE(node, w, t, m, h, p)                                     \
do {                                                                       \
    (node)->wild_child = (w);                                              \
    (node)->type = (t);                                                    \
    (node)->max_params = (m);                                              \
    (node)->handler = (h);                                                 \
    (node)->priority = (p);                                                \
} while (0)

#define INIT_NODE2(node, w, t, m, h, p, pb, pl, ib, il)                    \
do {                                                                       \
    INIT_NODE((node), (w), (t), (m), (h), (p));                            \
    COPY_STRING((node)->indices, (ib), (il));                              \
    COPY_STRING((node)->path, (pb), (pl));                                 \
} while (0)

#define COPY_CHILDREN(dst, src)                                            \
do {                                                                       \
    size_t n = sizeof(void *) * (src)->nchildren;                          \
                                                                           \
    (dst)->children = palloc(n);                                           \
    (dst)->nchildren = (src)->nchildren;                                   \
    (dst)->capacity = (src)->nchildren;                                    \
                                                                           \
    memcpy((dst)->children, (src)->children, n);                           \
} while (0)

#define INIT_CHILDREN(node, child)                                         \
do {                                                                       \
    if ((node)->nchildren) {                                               \
        pfree((node)->children);                                           \
        (node)->nchildren = 0;                                             \
        (node)->capacity = 0;                                              \
    }                                                                      \
    (node)->children = palloc(sizeof(void *) * 2);                         \
    (node)->capacity = 2;                                                  \
    (node)->nchildren = 1;                                                 \
    (node)->children[0] = (child);                                         \
} while (0)

#define APPEND_CHILD(node, child)                                          \
do {                                                                       \
    if ((node)->nchildren == 0) {                                          \
        INIT_CHILDREN((node), (child));                                    \
    } else {                                                               \
        size_t size;                                                       \
        if ((node)->nchildren == (node)->capacity) {                       \
            size = sizeof(void *) * (node)->capacity;                      \
            (node)->children = realloc((node)->children, 2 * size);        \
            (node)->capacity *= 2;                                         \
        }                                                                  \
        (node)->children[(node)->nchildren++] = (child);                   \
    }                                                                      \
} while (0)

#define MIN(x, y) ((x) <= (y) ? (x) : (y))

static int
pgrest_rtree_child_priority(pgrest_rtree_node_t *node, int pos)
{
    unsigned char        c;
    pgrest_rtree_node_t *tnode;
    int                  new_pos;
    int                  priority;

    node->children[pos]->priority++;
    priority = node->children[pos]->priority;

    new_pos = pos;
    while (new_pos > 0 && node->children[new_pos - 1]->priority < priority) {
        /* swap node positions */
        tnode = node->children[new_pos - 1];
        node->children[new_pos - 1] = node->children[new_pos];
        node->children[new_pos] = tnode;

        new_pos--;
    }

    if (new_pos != pos) {
        /* move index char */
        c = node->indices.base[pos];
        while (pos > new_pos) {
            node->indices.base[pos] = node->indices.base[pos - 1];
            pos--;
        }
        node->indices.base[new_pos] = c;
    }

    return new_pos;
}

static bool
pgrest_rtree_child_insert(pgrest_rtree_node_t *node,
                          int                  num_params,
                          pgrest_string_t      path,
                          pgrest_string_t      full_path,
                          void                *handler)
{
    unsigned char        c;
    pgrest_rtree_node_t *child;
    int                  i, max, end, offset = 0;

    for (i = 0, max = path.len; num_params > 0; i++) {
        c = path.base[i];
        if ( c != ':' && c != '*' ) {
            continue;
        }

        end = i + 1;
        while (end < max && path.base[end] != '/') {
            switch (path.base[end]) {
            default:
                end++;
                break;
            case ':':
            case '*':
                ereport(LOG, (errmsg(PGREST_PACKAGE " " "only one wildcard per "
                                     "path segment is allowed, has: \"%s\" in "
                                     "path \"%s\"", path.base + i, 
                                      full_path.base)));
                return false;
            }
        }

        if (node->nchildren) {
            ereport(LOG, (errmsg(PGREST_PACKAGE " " "wildcard route \"%s\" "
                                 "conflicts with existing children in path "
                                 "\"%s\"", path.base + i, full_path.base)));
            return false;
        }

        if (end - i < 2) {
            ereport(LOG, (errmsg(PGREST_PACKAGE " " "wildcards must"
                                 " be named with a non-empty name "
                                 "in path \"%s\"", full_path.base)));
            return false;
        }

        if (c == ':') {
            /* param */
            if (i > 0) {
                COPY_STRING(node->path, path.base + offset, i - offset);
                offset = i;
            }

            child = palloc0(sizeof(pgrest_rtree_node_t));
            INIT_NODE(child, false, PGREST_RTREE_NODE_PARAM, num_params, NULL, 0);

            INIT_CHILDREN(node, child);
            node->wild_child = true;

            node = child;
            node->priority++;

            num_params--;

            if (end < max) {
                COPY_STRING(node->path, path.base + offset, end - offset);
                offset = end;

                child = palloc0(sizeof(pgrest_rtree_node_t));
                INIT_NODE(child, false, PGREST_RTREE_NODE_STATIC, num_params, NULL, 1);

                INIT_CHILDREN(node, child);
                node = child;
            }
        } else {
            if (end != max || num_params > 1) {
                ereport(LOG, 
                        (errmsg(PGREST_PACKAGE " " "catch-all routes are only "
                                "allowed at the end of the path in path \"%s\""
                                , full_path.base)));
                return false;
            }

            if (node->path.len >0 && 
                node->path.base[node->path.len - 1] == '/') 
            {
                ereport(LOG, 
                        (errmsg(PGREST_PACKAGE " " "catch-all conflicts with "
                                "existing handler for the path segment root "
                                "in path \"%s\"", full_path.base)));
                return false;
            }

            i--;
            if (path.base[i] != '/') {
                ereport(LOG, 
                        (errmsg(PGREST_PACKAGE " " "no / before catch-all"
                                " in path \"%s\"", full_path.base)));
                return false;
            }

            COPY_STRING(node->path, path.base + offset, i - offset);

            /* first node: catchAll node with empty path */
            child = palloc0(sizeof(pgrest_rtree_node_t));
            INIT_NODE(child, true, PGREST_RTREE_NODE_CATCHALL, 1, NULL, 0);

            INIT_CHILDREN(node, child);
            COPY_STRING(node->indices, path.base + i, 1);

            node = child;
            node->priority++;

            /* second node: node holding the variable */
            child = palloc0(sizeof(pgrest_rtree_node_t));
            INIT_NODE(child, false, PGREST_RTREE_NODE_CATCHALL, 1, handler, 1);
            COPY_STRING(child->path, path.base + i, path.len - i);

            INIT_CHILDREN(node, child);
            return true;
        }
    }

    /* insert remaining path part and handler to the leaf */
    COPY_STRING(node->path, path.base + offset, path.len - offset);
    node->handler = handler;
    return true;
}

bool
pgrest_rtree_add(pgrest_rtree_node_t *node, pgrest_string_t path, void *handler)
{
    int                   i, j, max;
    pgrest_rtree_node_t  *child;
    unsigned char         c;
    bool                  found;
    pgrest_string_t       full_path  = path;
    int                   num_params = pgrest_count_params(path);

    node->priority++;

    /* Empty tree */
    if (node->path.len == 0 && node->nchildren == 0) {
        node->type = PGREST_RTREE_NODE_ROOT;
        if (!pgrest_rtree_child_insert(node, num_params, path,
                                        full_path, handler)) {
            return false;
        }
        return true;
    }

    for (;;) {
        if (num_params > node->max_params) {
            node->max_params = num_params;
        }

        /* Find the longest common prefix */
        max = MIN(path.len, node->path.len);
        for (i = 0; i < max && path.base[i] == node->path.base[i];) { 
            i++;
        }

        if (i < node->path.len) {
            child = palloc0(sizeof(pgrest_rtree_node_t));
            INIT_NODE2(child, node->wild_child, PGREST_RTREE_NODE_STATIC, 0, 
                       node->handler, node->priority - 1, node->path.base + i, 
                       node->path.len -i, node->indices.base, node->indices.len);
            COPY_CHILDREN(child, node);

            for (j = 0; j < child->nchildren; j++) {
                if (child->children[j]->max_params > child->max_params) {
                    child->max_params = child->children[j]->max_params;
                }
            }

            INIT_CHILDREN(node, child);
            INIT_NODE2(node, false, node->type, node->max_params, NULL, 
                       node->priority, path.base, i, node->path.base + i, 1);
        }

        /* Make new node a child of this node */
        if (i < path.len) {
            INIT_STRING(path, path.base + i, path.len - i);

            if (node->wild_child) {
                node = node->children[0];
                node->priority++;

                if (num_params > node->max_params) {
                    node->max_params = num_params;
                }
                num_params--;

                if (path.len >= node->path.len && 
                    !strncmp((char *)node->path.base, 
                              (char *)path.base, node->path.len) &&
                    (node->path.len >= path.len || 
                     path.base[node->path.len] == '/')) 
                {
                    continue;
                } else {
                    ereport(LOG, 
                            (errmsg(PGREST_PACKAGE " " "conflict"
                                    " with existing wildcard")));
                    return false;
                }
            }

            c = path.base[0];

            if (c == '/' && node->nchildren == 1 && 
                node->type == PGREST_RTREE_NODE_PARAM ) 
            { 
                node = node->children[0];
                node->priority++;
                continue;
            }

            found = false;
            for (j = 0; j < node->indices.len; j++) {
                if (c == node->indices.base[j]) {
                    j = pgrest_rtree_child_priority(node, j);
                    node = node->children[j];
                    found = true;
                    break;
                }
            }

            if (found) {
                continue;
            }

            if (c != ':' && c != '*') {
                APPEND_STRING(node->indices, &c, 1);

                child = palloc0(sizeof(pgrest_rtree_node_t));
                child->max_params = num_params;

                APPEND_CHILD(node, child);
                pgrest_rtree_child_priority(node, node->indices.len - 1);
                node = child;
            }

            if (!pgrest_rtree_child_insert(node, num_params, path,
                                             full_path, handler)) {
                 return false;
            }

            return true;
        } else if (i == path.len) {
            /* Make node a (in-path) leaf */
            if (node->handler != NULL) {
                ereport(LOG, 
                       (errmsg(PGREST_PACKAGE " " "a handler is already "
                               "registered for path \"%s\"", full_path.base)));
                return false;
            }
            node->handler = handler;
        }

        return true;;
    }

    return true;
}

bool
pgrest_rtree_find(pgrest_rtree_node_t  *node, 
                  pgrest_string_t       path,
                  void                **handler,
                  pgrest_param_t       *params, 
                  size_t               *num_params)
{
    unsigned char c;
    size_t        n;
    bool          found;
    int           i, end, max_params = *num_params;

    *num_params = 0;
    while (true) {
        if (path.len > node->path.len) {
            if (!strncmp((char *)path.base, 
                         (char *)node->path.base, 
                         node->path.len)) 
            {
                n = path.len - node->path.len;
                INIT_STRING(path, path.base + node->path.len, n);

                if (!node->wild_child) {
                    c = path.base[0];

                    found = false;
                    for (i = 0; i < node->indices.len; i++) {
                        if (c == node->indices.base[i]) {
                            node = node->children[i];
                            found = true;
                            break;
                        }
                    }

                    if (found) {
                        continue;
                    }

                    return false;
                }

                if (*num_params == max_params) {
                    ereport(LOG, 
                            (errmsg(PGREST_PACKAGE " " "too many parameters,"
                                    " max allowed:%d", max_params)));
                    return false;
                }

                /* handle wildcard child */
                node = node->children[0];
                switch (node->type) {
                case PGREST_RTREE_NODE_CATCHALL:
                    n = node->path.len - 2;
                    INIT_STRING(params[*num_params].value, path.base, path.len);
                    INIT_STRING(params[*num_params].key, node->path.base + 2, n);

                    (*num_params)++;
                    *handler = node->handler;
                    return true;
                case PGREST_RTREE_NODE_PARAM:
                    n = node->path.len - 1;
                    for (end = 0; end < path.len && path.base[end] != '/';) {
                        end++;
                    }

                    INIT_STRING(params[*num_params].value, path.base, end);
                    INIT_STRING(params[*num_params].key, node->path.base + 1, n);

                    (*num_params)++;
                    if (end < path.len) {
                        if (node->nchildren > 0) {
                            INIT_STRING(path, path.base + end, path.len - end);
                            node = node->children[0];
                            continue;
                        }
                        
                        return false;
                    }

                    *handler = node->handler;
                    return true;
                default:
                    ereport(LOG, 
                            (errmsg(PGREST_PACKAGE " " "invalid node type:%d",
                                    max_params)));
                    return false;
                }
            }
        } else if (path.len == node->path.len && 
                   !strncmp((char *)path.base, 
                            (char *)node->path.base, 
                            path.len)) {
            /* We should have reached the node containing the handler */
            *handler = node->handler;
            return true;
        }

        return false;
    }
}

void
pgrest_rtree_walker(pgrest_rtree_node_t *root, void (*walker) ())
{
    int i;

    for (i = 0; i < root->nchildren; i++) {
        walker(root->children[i]->handler);
        pgrest_rtree_walker(root->children[i], walker);
    }
}

pgrest_rtree_node_t *
pgrest_rtree_create(void)
{
    return palloc0(sizeof(pgrest_rtree_node_t));
}

void
pgrest_rtree_destroy(pgrest_rtree_node_t *root)
{
    int i;

    for (i = 0; i < root->nchildren; i++) {
        pgrest_rtree_destroy(root->children[i]);        
    }

    if (root->nchildren) {
        pfree(root->children);
    }

    if (root->path.base) {
        pfree(root->path.base);
    }

    if (root->indices.base) {
        pfree(root->indices.base);
    }

    pfree(root);
}
