#include "mongo_utils.h"
#include <stdio.h>
#include <string.h>

int mongo_init(mongo_context_t *ctx, const char *uri, const char *db_name, const char *collection_name) {
    if (!ctx || !uri || !db_name || !collection_name) {
        fprintf(stderr, "Error: Parámetros inválidos en mongo_init\n");
        return 0;
    }

    mongoc_init();
    
    // Limpiar el contexto antes de inicializar
    memset(ctx, 0, sizeof(mongo_context_t));
    
    ctx->uri = uri;
    ctx->db_name = db_name;
    ctx->collection_name = collection_name;
    
    // Imprimir información de conexión para depuración
    //printf("Intentando conectar a MongoDB:\n");
    //printf("URI: %s\n", uri);
    //printf("Base de datos: %s\n", db_name);
    //printf("Colección: %s\n", collection_name);
    
    // Crear cliente MongoDB con opciones
    bson_error_t error;
    mongoc_uri_t *uri_obj = mongoc_uri_new_with_error(uri, &error);
    if (!uri_obj) {
        fprintf(stderr, "Error al parsear URI de MongoDB: %s\n", error.message);
        return 0;
    }

    // Configurar opciones adicionales
    mongoc_uri_set_option_as_int32(uri_obj, "serverSelectionTimeoutMS", 5000);
    mongoc_uri_set_option_as_bool(uri_obj, "retryWrites", true);
    
    ctx->client = mongoc_client_new_from_uri(uri_obj);
    mongoc_uri_destroy(uri_obj);
    
    if (!ctx->client) {
        fprintf(stderr, "Error: No se pudo crear el cliente MongoDB\n");
        return 0;
    }
    
    // Verificar la conexión con ping
    bson_t *command = BCON_NEW("ping", BCON_INT32(1));
    bool retval = mongoc_client_command_simple(ctx->client, "admin", command, NULL, NULL, &error);
    bson_destroy(command);
    
    if (!retval) {
        fprintf(stderr, "Error al conectar con MongoDB: %s\n", error.message);
        mongoc_client_destroy(ctx->client);
        ctx->client = NULL;
        return 0;
    }
    
    //printf("Conexión exitosa a MongoDB\n");
    
    // Obtener base de datos y verificar permisos
    mongoc_database_t *database = mongoc_client_get_database(ctx->client, db_name);
    if (!database) {
        fprintf(stderr, "Error: No se pudo obtener la base de datos %s\n", db_name);
        mongoc_client_destroy(ctx->client);
        ctx->client = NULL;
        return 0;
    }

    // Obtener y verificar la colección
    ctx->collection = mongoc_database_get_collection(database, collection_name);
    mongoc_database_destroy(database);
    
    if (!ctx->collection) {
        fprintf(stderr, "Error: No se pudo obtener la colección %s\n", collection_name);
        mongoc_client_destroy(ctx->client);
        ctx->client = NULL;
        return 0;
    }

    // Verificar permisos de escritura en la colección con un comando más simple
    bson_t *test_doc = BCON_NEW("_id", BCON_UTF8("test"), "test", BCON_UTF8("test"));
    if (!mongoc_collection_insert_one(ctx->collection, test_doc, NULL, NULL, &error)) {
        fprintf(stderr, "Error: No tienes permisos de escritura en la colección %s: %s\n", 
                collection_name, error.message);
        bson_destroy(test_doc);
        mongoc_collection_destroy(ctx->collection);
        mongoc_client_destroy(ctx->client);
        ctx->collection = NULL;
        ctx->client = NULL;
        return 0;
    }

    // Eliminar el documento de prueba
    bson_t *delete_query = BCON_NEW("_id", BCON_UTF8("test"));
    mongoc_collection_delete_one(ctx->collection, delete_query, NULL, NULL, NULL);
    bson_destroy(delete_query);
    bson_destroy(test_doc);
    
    //printf("Colección verificada y accesible correctamente\n");
    return 1;
}

int mongo_save_address(mongo_context_t *ctx, const char *address, const char *private_key, const char *pattern) {
    if (!ctx || !ctx->collection) {
        fprintf(stderr, "Error: Contexto de MongoDB no inicializado correctamente\n");
        return 0;
    }

    bson_error_t error;
    bson_t *doc;
    time_t now;
    char timestamp[32];
    
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Asegurarnos de que todos los campos sean strings UTF-8 válidos
    if (!address || !pattern) {
        fprintf(stderr, "Error: Dirección o patrón inválidos\n");
        return 0;
    }

    doc = BCON_NEW(
        "address", BCON_UTF8(address),
        "private_key", BCON_UTF8(private_key ? private_key : ""),
        "pattern", BCON_UTF8(pattern),
        "created_at", BCON_UTF8(timestamp)
    );
    
    if (!doc) {
        fprintf(stderr, "Error: No se pudo crear el documento BSON\n");
        return 0;
    }
    
    //printf("Intentando guardar dirección en MongoDB...\n");
    
    if (!mongoc_collection_insert_one(ctx->collection, doc, NULL, NULL, &error)) {
        fprintf(stderr, "Error al guardar en MongoDB: %s\n", error.message);
        bson_destroy(doc);
        return 0;
    }
    
    //printf("Dirección guardada exitosamente en MongoDB\n");
    bson_destroy(doc);
    return 1;
}

void mongo_cleanup(mongo_context_t *ctx) {
    if (ctx) {
        if (ctx->collection) {
            mongoc_collection_destroy(ctx->collection);
            ctx->collection = NULL;
        }
        if (ctx->client) {
            mongoc_client_destroy(ctx->client);
            ctx->client = NULL;
        }
        mongoc_cleanup();
    }
} 