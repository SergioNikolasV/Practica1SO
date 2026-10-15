#ifndef SEARCH_H
#define SEARCH_H

#define SEARCH_MAX_PARAMETRO 128
#define SEARCH_MAX_FILAS 10
#define SEARCH_MAX_LONGITUD_FILA 1024
#define SEARCH_TAM_BLOQUE_DEFAULT SEARCH_MAX_FILAS
#define SEARCH_CSV_DEFAULT "dataset/archive/song_longevity.csv"

typedef struct SearchContext SearchContext;

typedef enum {
	SEARCH_OK = 0,
	SEARCH_SIN_RESULTADOS,
	SEARCH_ERROR
} SearchStatus;

typedef struct {
	int num_filas;
	int hay_mas;
	int codigo_error;
	char filas[SEARCH_MAX_FILAS][SEARCH_MAX_LONGITUD_FILA];
} SearchResult;

SearchContext *search_open(const char *ruta_csv);
void search_close(SearchContext *contexto);

SearchStatus search_track_id(SearchContext *contexto, const char *track_id,SearchResult *resultado);
SearchStatus search_genre_start(SearchContext *contexto, const char *genre, int tam_bloque, SearchResult *resultado);
SearchStatus search_track_catalog_start(SearchContext *contexto, SearchResult *resultado);
SearchStatus search_genres_start(SearchContext *contexto, SearchResult *resultado);
SearchStatus search_next(SearchContext *contexto, SearchResult *resultado);

#endif
