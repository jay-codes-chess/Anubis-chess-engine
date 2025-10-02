/*
	Anubis

	José Carlos Martínez Galán
*/

#include "Preprocesador.h"
#include "Tipos.h"
#include "Constantes.h"
#include <stdio.h>

// Ataques
// OJO: Idea - hacer todo esto constante, inicializándolo a lo bestia; por ejemplo: const UINT64 BB_ATAQUES_REY[64] = {BB_B8 | BB_A7 | BB_B7, etc...}, luego, para la función Atacado, crear un array constante de
//	TODOSLOSATAQUES, de manera que la primera comprobación sea de esa constante (de la casilla en cuestión) contra todas las piezas enemigas; si no hay coincidencia, salir directamente
UINT64	au64AtaquesTorre[64];
UINT64	au64AtaquesPeonB[64];
UINT64	au64AtaquesPeonN[64];

// Busqueda
UINT64				au64RepeticionesB[MAX_POSICIONES+4];
UINT64				au64RepeticionesN[MAX_POSICIONES+4];
TPosicion			aPilaPosiciones[MAX_POSICIONES+4];
TJugada				aPilaJugadas[MAX_JUGADAS];
TReloj				g_tReloj;
TJugada				ajugKillers[MAX_PLIES][2];
SINT32				as32PilaEvaluaciones[MAX_POSICIONES+4];	// Desde el punto de vista del blanco
TJugada				aJugParaIID[MAX_PLIES];
SINT32				as32Historia[64][64];
#if defined(REFUTACION)
UINT32				au32Refuta[15][64][15][64] = {0}; // [pieza][casilla][pieza][casilla]
#endif

// Bitboards
UINT64		au64Entre[64][64];
UINT64		au64AdyacenteH[64];
UINT64		au64Mask[64];
UINT64		au64HaciaElBorde[64][64];
UINT64		au64AreaPasadoB[64];			// Area que debe estar libre de peones enemigos para que el nuestro esté pasado
UINT64		au64AreaPasadoN[64];			// Area que debe estar libre de peones enemigos para que el nuestro esté pasado
UINT64		au64DireccionesDiag[64];		// Las (hasta) 4 casillas diagonales alrededor de cada casilla
UINT64		au64DireccionesRect[64];		// Idem para casillas en direcciones rectas
UINT64		au64Circunferencia2[64];		// Circunferencia exterior para rey
UINT64		au64FrontalReyBlanco[64];		// Las 3 casillas justo encima
UINT64		au64FrontalReyNegro[64];		// Las 3 casillas justo debajo
UINT64		au64ZonaReyExtendidaAbajo[64];	// Los ataques del rey más la fila inferior
UINT64		au64ZonaReyExtendidaArriba[64];	// Los ataques del rey más la fila superior
UINT64		au64ZonaKPKGanaB[64];			// Si el rey blanco está ahí en KPK, ganado
UINT64		au64ZonaKPKGanaN[64];			// Si el rey negro está ahí en KPK, ganado
UINT64		au64ZonaReyBNoLlega[64];		// Zona donde, si hay un peón negro, el rey blanco no llega a pararlo si no le toca
UINT64		au64ZonaReyNNoLlega[64];		// Zona donde, si hay un peón blanco, el rey negro no llega a pararlo si no le toca
UINT64		au64ZonaReyBNoLlegaConTurno[64]; // Zona donde, si hay un peón negro, el rey blanco no llega a pararlo aunque le toque
UINT64		au64ZonaReyNNoLlegaConTurno[64]; // Zona donde, si hay un peón blanco, el rey negro no llega a pararlo aunque le toque

// Tablero
SINT8		as8Direccion[64][64];
UINT8		au8DistCuadroPB[64][64];		// Distancia al cuadro de un peón blanco
UINT8		au8DistCuadroPN[64][64];		// [Peón][Rey]
UINT8		au8Distancia[64][64];			// Entre dos casillas
UINT8		au8Oposicion[64][64];			// 0: no oposición; 1: oposición; 2: oposición distante

// General
TDatosBusqueda	dbDatosBusqueda;
TDatosPocoUso	dpuDatos;
char		szMiNombre[256] = VERSION;
FILE		*pFLog;
FILE    *pFLogLMR;

// Hashing
UINT64			au64PeonB_Random[64];
UINT64			au64PeonN_Random[64];
UINT64			au64CaballoB_Random[64];
UINT64			au64CaballoN_Random[64];
UINT64			au64AlfilB_Random[64];
UINT64			au64AlfilN_Random[64];
UINT64			au64TorreB_Random[64];
UINT64			au64TorreN_Random[64];
UINT64			au64DamaB_Random[64];
UINT64			au64DamaN_Random[64];
UINT64			au64ReyB_Random[64];
UINT64			au64ReyN_Random[64];
UINT64			au64Enroques_Random[16];
UINT64			au64AlPaso_Random[65];
TNodoHash		*aTablaHash;	// La notación 'a' es por su uso (se usará como array, una vez inicializado)
TNodoHashEval	*aHashEval;		// Idem
TNodoHash		*aHashQSCJ;		// Para QSearchConJaques()
UINT32			u32TamTablaHash;
UINT32			u32TamHashPeones;
UINT32			u32TamHashEval;
UINT32			u32TamHashQSCJ;

// EGTB's
UINT32		u32MaxPiezasTB = 0;

// Comunicación con el thread del motor
volatile UINT32         u32UltComandoEnviado = 0;
volatile TComandoMotor  acmListaComandos[MAX_COMANDOS];
