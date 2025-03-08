#ifndef MONGO_UTILS_H
#define MONGO_UTILS_H

#include <libmongoc-1.0/mongoc.h>
#include <libbson-1.0/bson.h>

// Estructura para la configuración de MongoDB
typedef struct {
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    const char *uri;
    const char *db_name;
    const char *collection_name;
} mongo_context_t;

// Funciones para manejar MongoDB
int mongo_init(mongo_context_t *ctx, const char *uri, const char *db_name, const char *collection_name);
int mongo_save_address(mongo_context_t *ctx, const char *address, const char *private_key, const char *pattern);
void mongo_cleanup(mongo_context_t *ctx);

#endif /* MONGO_UTILS_H */ 