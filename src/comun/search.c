#define _POSIX_C_SOURCE 200809L

#include "search.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	CURSOR_NINGUNO,
	CURSOR_GENRE,
	CURSOR_TRACKS,
	CURSOR_GENRES
};

struct SearchContext {
	FILE *csv;
	char genre[SEARCH_MAX_PARAMETRO];
	char fila_pendiente[SEARCH_MAX_LONGITUD_FILA];
	int hay_fila_pendiente;
	int tam_bloque;
	int tipo_cursor;
	char **genres;
	size_t num_genres;
	size_t indice_genre;
};

static FILE *abrir_csv(const char *ruta_csv) {
	if (ruta_csv != NULL) {
		return fopen(ruta_csv, "r");
	}

	FILE *csv = fopen(SEARCH_CSV_DEFAULT, "r");
	if (csv == NULL) {
		csv = fopen("../../" SEARCH_CSV_DEFAULT, "r");
	}
	return csv;
}

static void quitar_fin_linea(char *fila) {
	size_t longitud = strlen(fila);
	while (longitud > 0 && (fila[longitud - 1] == '\n' || fila[longitud - 1] == '\r')) {
		fila[--longitud] = '\0';
	}
}

static int dividir_claves(char *fila, char **track_id, char **genre) {
	char *separador = strchr(fila, ',');
	if (separador == NULL) {
		return -1;
	}

	*separador = '\0';
	*track_id = fila;
	*genre = separador + 1;
	separador = strchr(*genre, ',');
	if (separador != NULL) {
		*separador = '\0';
	}
	return 0;
}

static int genre_iguales(const char *a, const char *b) {
	while (*a != '\0' && *b != '\0') {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
			return 0;
		}
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static int cabecera_valida(const char *cabecera) {
	return strncmp(cabecera, "track_id,genre,", 15) == 0;
}

static int volver_al_inicio(SearchContext *contexto) {
	char cabecera[SEARCH_MAX_LONGITUD_FILA];

	rewind(contexto->csv);
	clearerr(contexto->csv);
	if (fgets(cabecera, sizeof(cabecera), contexto->csv) == NULL || !cabecera_valida(cabecera)) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static void limpiar_resultado(SearchResult *resultado) {
	memset(resultado, 0, sizeof(*resultado));
}

static void limpiar_cursor(SearchContext *contexto) {
	for (size_t i = 0; i < contexto->num_genres; i++) {
		free(contexto->genres[i]);
	}
	free(contexto->genres);
	contexto->genres = NULL;
	contexto->num_genres = 0;
	contexto->indice_genre = 0;
	contexto->hay_fila_pendiente = 0;
	contexto->tipo_cursor = CURSOR_NINGUNO;
}

static SearchStatus error_resultado(SearchResult *resultado, int codigo_error) {
	limpiar_resultado(resultado);
	resultado->codigo_error = codigo_error == 0 ? EIO : codigo_error;
	errno = resultado->codigo_error;
	return SEARCH_ERROR;
}

static int leer_fila(SearchContext *contexto, char *fila) {
	if (fgets(fila, SEARCH_MAX_LONGITUD_FILA, contexto->csv) == NULL) {
		return ferror(contexto->csv) ? -1 : 0;
	}
	if (strchr(fila, '\n') == NULL && !feof(contexto->csv)) {
		int caracter;
		while ((caracter = fgetc(contexto->csv)) != '\n' && caracter != EOF) {
		}
		errno = EOVERFLOW;
		return -1;
	}
	quitar_fin_linea(fila);
	return 1;
}

static int siguiente_genre(SearchContext *contexto, char *fila) {
	char claves[SEARCH_MAX_LONGITUD_FILA];
	char *track_id;
	char *genre;
	int lectura;

	while ((lectura = leer_fila(contexto, fila)) == 1) {
		snprintf(claves, sizeof(claves), "%s", fila);
		if (dividir_claves(claves, &track_id, &genre) == 0 &&
			genre_iguales(genre, contexto->genre)) {
			return 1;
		}
	}
	return lectura;
}

static int siguiente_catalogo_tracks(SearchContext *contexto, char *fila) {
	char registro[SEARCH_MAX_LONGITUD_FILA];
	char *track_id;
	char *genre;
	int lectura;

	while ((lectura = leer_fila(contexto, registro)) == 1) {
		if (dividir_claves(registro, &track_id, &genre) == 0) {
			snprintf(fila, SEARCH_MAX_LONGITUD_FILA, "%s,%s", track_id, genre);
			return 1;
		}
	}
	return lectura;
}

static int siguiente_item(SearchContext *contexto, char *fila) {
	switch (contexto->tipo_cursor) {
		case CURSOR_GENRE:
			return siguiente_genre(contexto, fila);
		case CURSOR_TRACKS:
			return siguiente_catalogo_tracks(contexto, fila);
		case CURSOR_GENRES:
			if (contexto->indice_genre >= contexto->num_genres) {
				return 0;
			}
		snprintf(fila, SEARCH_MAX_LONGITUD_FILA, "%s",
				 contexto->genres[contexto->indice_genre++]);
			return 1;
		default:
			errno = EINVAL;
			return -1;
	}
}

static SearchStatus leer_bloque(SearchContext *contexto, SearchResult *resultado) {
	char fila[SEARCH_MAX_LONGITUD_FILA];

	limpiar_resultado(resultado);
	while (resultado->num_filas < contexto->tam_bloque) {
		int lectura;
		if (contexto->hay_fila_pendiente) {
			snprintf(fila, sizeof(fila), "%s", contexto->fila_pendiente);
			contexto->hay_fila_pendiente = 0;
			lectura = 1;
		} else {
			lectura = siguiente_item(contexto, fila);
		}

		if (lectura == -1) {
			contexto->tipo_cursor = CURSOR_NINGUNO;
			return error_resultado(resultado, errno);
		}
		if (lectura == 0) {
			break;
		}
		snprintf(resultado->filas[resultado->num_filas],
				 sizeof(resultado->filas[resultado->num_filas]), "%s", fila);
		resultado->num_filas++;
	}

	if (resultado->num_filas == 0) {
		contexto->tipo_cursor = CURSOR_NINGUNO;
		return SEARCH_SIN_RESULTADOS;
	}

	int lectura = siguiente_item(contexto, contexto->fila_pendiente);
	if (lectura == -1) {
		contexto->tipo_cursor = CURSOR_NINGUNO;
		return error_resultado(resultado, errno);
	}
	contexto->hay_fila_pendiente = lectura == 1;
	resultado->hay_mas = contexto->hay_fila_pendiente;
	if (!contexto->hay_fila_pendiente) {
		contexto->tipo_cursor = CURSOR_NINGUNO;
	}
	return SEARCH_OK;
}

SearchContext *search_open(const char *ruta_csv) {
	FILE *csv = abrir_csv(ruta_csv);
	if (csv == NULL) {
		return NULL;
	}

	SearchContext *contexto = calloc(1, sizeof(*contexto));
	if (contexto == NULL) {
		fclose(csv);
		return NULL;
	}
	contexto->csv = csv;

	char cabecera[SEARCH_MAX_LONGITUD_FILA];
	if (fgets(cabecera, sizeof(cabecera), csv) == NULL || !cabecera_valida(cabecera)) {
		fclose(csv);
		free(contexto);
		errno = EINVAL;
		return NULL;
	}
	return contexto;
}

void search_close(SearchContext *contexto) {
	if (contexto != NULL) {
		limpiar_cursor(contexto);
		fclose(contexto->csv);
		free(contexto);
	}
}

SearchStatus search_track_id(SearchContext *contexto, const char *track_id,
							 SearchResult *resultado) {
	char fila[SEARCH_MAX_LONGITUD_FILA];
	char claves[SEARCH_MAX_LONGITUD_FILA];
	char *id_csv;
	char *genre_csv;
	int lectura;

	if (contexto != NULL) {
		limpiar_cursor(contexto);
	}
	if (contexto == NULL || track_id == NULL || resultado == NULL || track_id[0] == '\0') {
		if (resultado != NULL) {
			return error_resultado(resultado, EINVAL);
		}
		errno = EINVAL;
		return SEARCH_ERROR;
	}
	limpiar_resultado(resultado);
	if (volver_al_inicio(contexto) == -1) {
		return error_resultado(resultado, errno);
	}

	while ((lectura = leer_fila(contexto, fila)) == 1) {
		snprintf(claves, sizeof(claves), "%s", fila);
		if (dividir_claves(claves, &id_csv, &genre_csv) == 0 && strcmp(id_csv, track_id) == 0) {
			snprintf(resultado->filas[0], sizeof(resultado->filas[0]), "%s", fila);
			resultado->num_filas = 1;
			return SEARCH_OK;
		}
	}

	if (lectura == -1) {
		return error_resultado(resultado, errno);
	}
	return SEARCH_SIN_RESULTADOS;
}

SearchStatus search_genre_start(SearchContext *contexto, const char *genre,
								int tam_bloque, SearchResult *resultado) {
	if (contexto != NULL) {
		limpiar_cursor(contexto);
	}
	if (contexto == NULL || genre == NULL || resultado == NULL || genre[0] == '\0' ||
		strlen(genre) >= SEARCH_MAX_PARAMETRO) {
		if (resultado != NULL) {
			return error_resultado(resultado, EINVAL);
		}
		errno = EINVAL;
		return SEARCH_ERROR;
	}

	if (volver_al_inicio(contexto) == -1) {
		return error_resultado(resultado, errno);
	}

	snprintf(contexto->genre, sizeof(contexto->genre), "%s", genre);
	contexto->tam_bloque = tam_bloque <= 0 || tam_bloque > SEARCH_MAX_FILAS
							   ? SEARCH_TAM_BLOQUE_DEFAULT
							   : tam_bloque;
	contexto->tipo_cursor = CURSOR_GENRE;
	return leer_bloque(contexto, resultado);
}

static int comparar_genres(const void *a, const void *b) {
	const char *const *genre_a = a;
	const char *const *genre_b = b;
	return strcmp(*genre_a, *genre_b);
}

static int agregar_genre(SearchContext *contexto, const char *genre) {
	for (size_t i = 0; i < contexto->num_genres; i++) {
		if (genre_iguales(contexto->genres[i], genre)) {
			return 0;
		}
	}

	char **nuevos = realloc(contexto->genres,
							(contexto->num_genres + 1) * sizeof(*contexto->genres));
	if (nuevos == NULL) {
		return -1;
	}
	contexto->genres = nuevos;
	contexto->genres[contexto->num_genres] = strdup(genre);
	if (contexto->genres[contexto->num_genres] == NULL) {
		return -1;
	}
	contexto->num_genres++;
	return 0;
}

SearchStatus search_track_catalog_start(SearchContext *contexto, SearchResult *resultado) {
	if (contexto == NULL || resultado == NULL) {
		if (resultado != NULL) {
			return error_resultado(resultado, EINVAL);
		}
		errno = EINVAL;
		return SEARCH_ERROR;
	}

	limpiar_resultado(resultado);
	limpiar_cursor(contexto);
	if (volver_al_inicio(contexto) == -1) {
		return error_resultado(resultado, errno);
	}
	contexto->tipo_cursor = CURSOR_TRACKS;
	contexto->tam_bloque = SEARCH_MAX_FILAS;
	return leer_bloque(contexto, resultado);
}

SearchStatus search_genres_start(SearchContext *contexto, SearchResult *resultado) {
	char fila[SEARCH_MAX_LONGITUD_FILA];
	char *track_id;
	char *genre;
	int lectura;

	if (contexto == NULL || resultado == NULL) {
		if (resultado != NULL) {
			return error_resultado(resultado, EINVAL);
		}
		errno = EINVAL;
		return SEARCH_ERROR;
	}

	limpiar_resultado(resultado);
	limpiar_cursor(contexto);
	if (volver_al_inicio(contexto) == -1) {
		return error_resultado(resultado, errno);
	}
	while ((lectura = leer_fila(contexto, fila)) == 1) {
		if (dividir_claves(fila, &track_id, &genre) == 0 && genre[0] != '\0' &&
			agregar_genre(contexto, genre) == -1) {
			int codigo_error = errno == 0 ? ENOMEM : errno;
			limpiar_cursor(contexto);
			return error_resultado(resultado, codigo_error);
		}
	}
	if (lectura == -1) {
		return error_resultado(resultado, errno);
	}
	if (contexto->num_genres == 0) {
		limpiar_resultado(resultado);
		return SEARCH_SIN_RESULTADOS;
	}

	qsort(contexto->genres, contexto->num_genres, sizeof(*contexto->genres), comparar_genres);
	contexto->tipo_cursor = CURSOR_GENRES;
	contexto->tam_bloque = SEARCH_MAX_FILAS;
	return leer_bloque(contexto, resultado);
}

SearchStatus search_next(SearchContext *contexto, SearchResult *resultado) {
	if (contexto == NULL || resultado == NULL) {
		if (resultado != NULL) {
			return error_resultado(resultado, EINVAL);
		}
		errno = EINVAL;
		return SEARCH_ERROR;
	}
	if (contexto->tipo_cursor == CURSOR_NINGUNO || !contexto->hay_fila_pendiente) {
		return error_resultado(resultado, EINVAL);
	}
	return leer_bloque(contexto, resultado);
}
