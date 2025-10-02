
#pragma once

#include "Preprocesador.h"
#include "Tipos.h"
#include "Constantes.h"
#include <stdio.h>
#include <stdlib.h>

//
// EGTB (chikki)
//
extern int use_tb;
static int TBlargest;

// Ataques
extern UINT64	au64AtaquesTorre[64];
extern UINT64	au64AtaquesPeonB[64];
extern UINT64	au64AtaquesPeonN[64];

// Busqueda
extern UINT64		au64RepeticionesB[MAX_POSICIONES+4];
extern UINT64		au64RepeticionesN[MAX_POSICIONES+4];
extern TPosicion	aPilaPosiciones[MAX_POSICIONES+4];
extern TJugada		aPilaJugadas[MAX_JUGADAS];
extern TReloj		g_tReloj;
extern TJugada		ajugKillers[MAX_PLIES][2];
extern SINT32		as32PilaEvaluaciones[MAX_POSICIONES+4];	// Desde el punto de vista del blanco
extern TJugada		aJugParaIID[MAX_PLIES];
extern SINT32		as32Historia[64][64];
#if defined(REFUTACION)
static UINT32		au32Refuta[15][64][15][64]; // [pieza][casilla][pieza][casilla]
#endif

// Bitboards
extern UINT64		au64Entre[64][64];
extern UINT64		au64AdyacenteH[64];
extern UINT64		au64Mask[64];
extern UINT64		au64HaciaElBorde[64][64];
extern UINT64		au64AreaPasadoB[64];			// Area que debe estar libre de peones enemigos para que el nuestro esté pasado
extern UINT64		au64AreaPasadoN[64];			// Area que debe estar libre de peones enemigos para que el nuestro esté pasado
extern UINT64		au64DireccionesDiag[64];		// Las (hasta) 4 casillas diagonales alrededor de cada casilla
extern UINT64		au64DireccionesRect[64];		// Idem para casillas en direcciones rectas
extern UINT64		au64Circunferencia2[64];		// Circunferencia exterior para rey
extern UINT64		au64FrontalReyBlanco[64];		// Las 3 casillas justo encima
extern UINT64		au64FrontalReyNegro[64];		// Las 3 casillas justo debajo
extern UINT64		au64ZonaReyExtendidaAbajo[64];	// Los ataques del rey más la fila inferior
extern UINT64		au64ZonaReyExtendidaArriba[64];	// Los ataques del rey más la fila superior
extern UINT64		au64ZonaKPKGanaB[64];			// Si el rey blanco está ahí en KPK, ganado
extern UINT64		au64ZonaKPKGanaN[64];			// Si el rey negro está ahí en KPK, ganado
extern UINT64		au64ZonaReyBNoLlega[64];		// Zona donde, si hay un peón negro, el rey blanco no llega a pararlo si no le toca
extern UINT64		au64ZonaReyNNoLlega[64];		// Zona donde, si hay un peón blanco, el rey negro no llega a pararlo si no le toca
extern UINT64		au64ZonaReyBNoLlegaConTurno[64]; // Zona donde, si hay un peón negro, el rey blanco no llega a pararlo aunque le toque
extern UINT64		au64ZonaReyNNoLlegaConTurno[64]; // Zona donde, si hay un peón blanco, el rey negro no llega a pararlo aunque le toque

// Tablero
extern SINT8		as8Direccion[64][64];
extern UINT8		au8DistCuadroPB[64][64];		// Distancia al cuadro de un peón blanco
extern UINT8		au8DistCuadroPN[64][64];		// [Peón][Rey]
extern UINT8		au8Distancia[64][64];			// Entre dos casillas
extern UINT8		au8Oposicion[64][64];			// 0: no oposición; 1: oposición; 2: oposición distante

// General
extern TDatosBusqueda	dbDatosBusqueda;
extern TDatosPocoUso	dpuDatos;
extern UINT32		u32NumJugadasLibroB;
extern UINT32		u32NumJugadasLibroN;
extern char			szMiNombre[256];
extern FILE			*pFLog;
extern FILE     *pFLogLMR;

// Hashing
extern UINT64			au64PeonB_Random[64];
extern UINT64			au64PeonN_Random[64];
extern UINT64			au64CaballoB_Random[64];
extern UINT64			au64CaballoN_Random[64];
extern UINT64			au64AlfilB_Random[64];
extern UINT64			au64AlfilN_Random[64];
extern UINT64			au64TorreB_Random[64];
extern UINT64			au64TorreN_Random[64];
extern UINT64			au64DamaB_Random[64];
extern UINT64			au64DamaN_Random[64];
extern UINT64			au64ReyB_Random[64];
extern UINT64			au64ReyN_Random[64];
extern UINT64			au64Enroques_Random[16];
extern UINT64			au64AlPaso_Random[65];
extern TNodoHash		*aTablaHash;
extern TNodoHashEval	*aHashEval;		
extern TNodoHash		*aHashQSCJ;		// Para QSearchConJaques()
extern UINT32			u32TamTablaHash;
extern UINT32			u32TamHashPeones;
extern UINT32			u32TamHashEval;
extern UINT32			u32TamHashQSCJ;

// EGTB's
extern UINT32			u32MaxPiezasTB;

// Comunicación con el thread del motor
extern volatile UINT32        u32UltComandoEnviado;
extern volatile TComandoMotor acmListaComandos[MAX_COMANDOS];
