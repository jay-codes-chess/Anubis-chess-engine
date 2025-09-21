#pragma once

#include "Preprocesador.h"
#include "Tipos.h"
#include "Constantes.h"
#include "Variables.h"
#include <assert.h>
#include <memory.h>
#include "Funciones.h"
#include "tbprobe.h"
#include "nnue.h"

/*
 *
 * Búsqueda
 *
 */
static INLINE BOOL Bus_DoyMatePronto(SINT32 s32Eval)
{
	return(s32Eval >= +INFINITO - 30000);
}
static INLINE BOOL Bus_ReciboMatePronto(SINT32 s32Eval)
{
	return(s32Eval <= -INFINITO + 30000);
}
static INLINE void Bus_ActualizarPV(TJugada jug, SINT32 s32Ply)
{
	dbDatosBusqueda.ajugPV[s32Ply][0] = jug;
	memcpy(&(dbDatosBusqueda.ajugPV[s32Ply][1]), &(dbDatosBusqueda.ajugPV[s32Ply + 1][0]), sizeof(TJugada) * (MAX_PLIES - s32Ply));
	dbDatosBusqueda.ajugPV[s32Ply + 1][0] = JUGADA_NULA;
}

/*
 *
 * Hashing
 *
 */

//
// Get
//
static INLINE UINT32 Hash_GetProf(TNodoHash * phs)
{
	return((UINT32)phs->u8Prof);
}
static INLINE BOOL Hash_GetIntentarNull(TNodoHash * phs)
{
	assert((BOOL)(phs->u8Flags >> 4) == TRUE || (BOOL)(phs->u8Flags >> 4) == FALSE);
	return((BOOL)(phs->u8Flags >> 4));
}
static INLINE BOOL Hash_GetEsExacto(TNodoHash * phs)
{
	return((BOOL)((phs->u8Flags & (UINT8)BOUND_EXACTO) >> 1));
}
static INLINE BOOL Hash_GetEsUBound(TNodoHash * phs)
{
	return((BOOL)((phs->u8Flags & (UINT8)BOUND_UPPER) >> 2));
}
static INLINE BOOL Hash_GetEsLBound(TNodoHash * phs)
{
	return((BOOL)((phs->u8Flags & (UINT8)BOUND_LOWER) >> 3));
}
static INLINE int Hash_GetBound(TNodoHash* phs)
{
	return(phs->u8Flags & (BOUND_EXACTO | BOUND_UPPER | BOUND_LOWER));
}
static INLINE UINT32 Hash_GetTurno(TNodoHash * phs)
{
	return((UINT32)(phs->u8Flags & (UINT8)0x01));
}
static INLINE UINT32 Hash_GetMaterial(TNodoHash * phs)
{
	return((UINT32)(phs->u16Material));
}
static INLINE int EvalToTT(int eval, int ply)
{
	if (Bus_DoyMatePronto(eval))
		eval += ply;
	else if (Bus_ReciboMatePronto(eval))
		eval -= ply;

	return eval;
}
static INLINE int EvalFromTT(int eval, int ply)
{
	if (Bus_DoyMatePronto(eval))
		eval -= ply;
	else if (Bus_ReciboMatePronto(eval))
		eval += ply;

	return eval;
}

//
// Set
//
static INLINE void Hash_SetProf(TNodoHash * phs, UINT32 u32Prof)
{
	assert(u32Prof < 256);
	phs->u8Prof = (UINT8)u32Prof;
	assert(Hash_GetProf(phs) == u32Prof);
}
static INLINE void Hash_SetIntentarNull(TNodoHash * phs)
{
	phs->u8Flags |= (UINT8)0x10;
	assert(Hash_GetIntentarNull(phs));
}
static INLINE void Hash_SetExacto(TNodoHash * phs)
{
	phs->u8Flags &= (UINT8)0xF1;
	phs->u8Flags |= (UINT8)BOUND_EXACTO;
	assert(Hash_GetEsExacto(phs));
}
static INLINE void Hash_SetUBound(TNodoHash * phs)
{
	phs->u8Flags &= (UINT8)0xF1;
	phs->u8Flags |= (UINT8)BOUND_UPPER;
	assert(Hash_GetEsUBound(phs));
}
static INLINE void Hash_SetLBound(TNodoHash * phs)
{
	phs->u8Flags &= (UINT8)0xF1;
	phs->u8Flags |= (UINT8)BOUND_LOWER;
	assert(Hash_GetEsLBound(phs));
}
static INLINE void Hash_SetTurno(TNodoHash * phs, UINT32 u32Turno)
{
	assert(u32Turno == BLANCAS || u32Turno == NEGRAS);
	phs->u8Flags &= (UINT8)0xFE;
	phs->u8Flags |= (UINT8)u32Turno;
	assert(Hash_GetTurno(phs) == u32Turno);
}
static INLINE void Hash_SetMaterial(TNodoHash * phs, UINT32 u32Material)
{
	assert(u32Material == (UINT16)u32Material);
	phs->u16Material = (UINT16)u32Material;
	assert(u32Material == Hash_GetMaterial(phs));
}

/*
 *
 * Posición
 *
 */

//
// Get (000T JNRC)
//
static INLINE UINT32 Pos_GetTurno(TPosicion * pPos)
{
	return((UINT32)pPos->u8Turno);
}
static INLINE UINT32 Pos_GetJaqueado(TPosicion * pPos)
{
	return((UINT32)pPos->u8Jaqueado);
}
static INLINE UINT32 Pos_GetIntentarNull(TPosicion * pPos)
{
	return((UINT32)pPos->u8IntentarNull);
}
static INLINE UINT32 Pos_GetCaptura(TPosicion * pPos)
{
	return((UINT32)(pPos->s32EvalMaterial != (pPos-1)->s32EvalMaterial));
}
static INLINE UINT32 Pos_GetRecaptura(TPosicion * pPos)
{
	return((UINT32)(Pos_GetCaptura(pPos) && Pos_GetCaptura(pPos - 1)));
}
// Enroques
static INLINE UINT32 Pos_GetPuedeCortoB(TPosicion * pPos)
{
	return((pPos->u8Enroques)&1);
}
static INLINE UINT32 Pos_GetPuedeLargoB(TPosicion * pPos)
{
	return((pPos->u8Enroques)&2);
}
static INLINE UINT32 Pos_GetPuedeCortoN(TPosicion * pPos)
{
	return((pPos->u8Enroques)&4);
}
static INLINE UINT32 Pos_GetPuedeLargoN(TPosicion * pPos)
{
	return((pPos->u8Enroques)&8);
}

#define FAS_GENERAL_CON_DAMAS		(UINT32)0
#define FAS_GENERAL_SIN_DAMAS		(UINT32)14
#define FAS_APERTURA				(UINT32)1
#define FAS_MED_JUG_INI				(UINT32)2
#define FAS_MED_JUG_FIN				(UINT32)3
#define	FAS_FINAL					(UINT32)4
#define FAS_FINAL_PESADAS			(UINT32)5
#define FAS_FINAL_DAMAS				(UINT32)6
#define FAS_FINAL_TORRES_Y_MENORES	(UINT32)7
#define FAS_FINAL_TORRES			(UINT32)8
#define	FAS_FINAL_MENORES			(UINT32)9
#define FAS_FINAL_ALFILES			(UINT32)10
#define FAS_FINAL_CABALLOS			(UINT32)11
#define	FAS_FINAL_PEONES			(UINT32)12
#define FAS_FINAL_SIN_PEONES		(UINT32)13

static INLINE UINT32 Pos_GetFasePartida(TPosicion * pPos)
{
	UINT32 u32Patron = 0;

	// Más de 10 peones y más de 9 piezas -> APERTURA o MED_JUG_INI
	if ((pPos->u8NumPeonesB + pPos->u8NumPeonesN) > 10 && (pPos->u8NumPiezasB + pPos->u8NumPiezasN) > 9)
	{
		if (dbDatosBusqueda.u32NumPlyPartida < 18)
			return(FAS_APERTURA);
		return(FAS_MED_JUG_INI);
	}

	// Más de 6 peones y más de 5 piezas -> MED_JUG_FIN
	// 03/02/25 Cambio un poco estas condiciones
	if ((pPos->u8NumPeonesB + pPos->u8NumPeonesN) > 6 && (pPos->u8NumPiezasB + pPos->u8NumPiezasN) > 6)
		return(FAS_MED_JUG_FIN);
	if ((pPos->u8NumPeonesB + pPos->u8NumPeonesN) > 6 && (pPos->u8NumPiezasB + pPos->u8NumPiezasN) > 5 && (pPos->u64DamasB || pPos->u64DamasN))
		return(FAS_MED_JUG_FIN);

	// Patrón:
	//	1 - peones
	//	2 - caballos
	//	4 - alfiles
	//	8 - torres
	// 16 - damas
	if (pPos->u8NumPeonesB || pPos->u8NumPeonesN)
		u32Patron |= 1;
	if (pPos->u64CaballosB || pPos->u64CaballosN)
		u32Patron |= 2;
	if (pPos->u64AlfilesB || pPos->u64AlfilesN)
		u32Patron |= 4;
	if (pPos->u64TorresB || pPos->u64TorresN)
		u32Patron |= 8;
	if (pPos->u64DamasB || pPos->u64DamasN)
		u32Patron |= 16;
	switch (u32Patron)
	{
		case 1:								// Sólo hay peones
			return(FAS_FINAL_PEONES);
		case 1 | 2:							// Caballos y peones
			return(FAS_FINAL_CABALLOS);
		case 1 | 4:							// Alfiles y peones
			return(FAS_FINAL_ALFILES);
		case 1 | 2 | 4:						// Alfiles, caballos y peones
			return(FAS_FINAL_MENORES);
		case 1 | 8:							// Torres y peones
			return(FAS_FINAL_TORRES);
		case 1 | 2 | 8:						// Torres, caballos y peones
		case 1 | 4 | 8:						// Torres, alfiles y peones
		case 1 | 2 | 4 | 8:					// Torres, alfiles, caballos y peones
			return(FAS_FINAL_TORRES_Y_MENORES);
		case 1 | 16:						// Damas y peones
			return(FAS_FINAL_DAMAS);
		case 1 | 8 | 16:					// Damas, torres y peones
			return(FAS_FINAL_PESADAS);
		default:
			if (u32Patron & 1)				// Con peones
				return(FAS_FINAL);
			else							// Sin peones
				return(FAS_FINAL_SIN_PEONES);
	}
}

//
// Set
//
static INLINE void Pos_SetTurnoB(TPosicion * pPos)
{
	pPos->u8Turno = BLANCAS;
	assert(Pos_GetTurno(pPos) == BLANCAS);
}
static INLINE void Pos_SetTurnoN(TPosicion * pPos)
{
	pPos->u8Turno = NEGRAS;
	assert(Pos_GetTurno(pPos) == NEGRAS);
}
static INLINE void Pos_CambiarTurno(TPosicion * pPos)
{
	pPos->u8Turno = (UINT8)!pPos->u8Turno;
}
static INLINE void Pos_SetJaqueado(TPosicion * pPos)
{
	pPos->u8Jaqueado = (UINT8)1;
	assert(Pos_GetJaqueado(pPos));
}
static INLINE void Pos_SetNoJaqueado(TPosicion * pPos)
{
	pPos->u8Jaqueado = (UINT8)0;
	assert(!Pos_GetJaqueado(pPos));
}
static INLINE void Pos_SetIntentarNull(TPosicion * pPos)
{
	pPos->u8IntentarNull = (UINT8)1;
	assert(Pos_GetIntentarNull(pPos));
}
static INLINE void Pos_SetNoIntentarNull(TPosicion * pPos)
{
	pPos->u8IntentarNull = (UINT8)0;
	assert(!Pos_GetIntentarNull(pPos));
}
// Enroques
static INLINE void Pos_SetCortoB(TPosicion * pPos)
{
	pPos->u8Enroques |= (UINT8)1;
	assert(Pos_GetPuedeCortoB(pPos));
}
static INLINE void Pos_SetLargoB(TPosicion * pPos)
{
	pPos->u8Enroques |= (UINT8)2;
	assert(Pos_GetPuedeLargoB(pPos));
}
static INLINE void Pos_SetCortoN(TPosicion * pPos)
{
	pPos->u8Enroques |= (UINT8)4;
	assert(Pos_GetPuedeCortoN(pPos));
}
static INLINE void Pos_SetLargoN(TPosicion * pPos)
{
	pPos->u8Enroques |= (UINT8)8;
	assert(Pos_GetPuedeLargoN(pPos));
}
static INLINE void Pos_SetNoEnroquesB(TPosicion * pPos)
{
	pPos->u8Enroques &= (UINT8)0xFC; // 1111 1100
	assert(!Pos_GetPuedeCortoB(pPos));
	assert(!Pos_GetPuedeLargoB(pPos));
}
static INLINE void Pos_SetNoEnroqueCortoB(TPosicion * pPos)
{
	pPos->u8Enroques &= (UINT8)0xFE; // 1111 1110
	assert(!Pos_GetPuedeCortoB(pPos));
}
static INLINE void Pos_SetNoEnroqueLargoB(TPosicion * pPos)
{
	pPos->u8Enroques &= (UINT8)0xFD; // 1111 1101
	assert(!Pos_GetPuedeLargoB(pPos));
}
static INLINE void Pos_SetNoEnroquesN(TPosicion * pPos)
{
	pPos->u8Enroques &= (UINT8)0xF3; // 1111 0011
	assert(!Pos_GetPuedeCortoN(pPos));
	assert(!Pos_GetPuedeLargoN(pPos));
}
static INLINE void Pos_SetNoEnroqueCortoN(TPosicion * pPos)
{
	pPos->u8Enroques &= (UINT8)0xFB; // 1111 1011
	assert(!Pos_GetPuedeCortoN(pPos));
}
static INLINE void Pos_SetNoEnroqueLargoN(TPosicion * pPos)
{
	pPos->u8Enroques &= (UINT8)0xF7; // 1111 0111
	assert(!Pos_GetPuedeLargoN(pPos));
}
static INLINE void Pos_SetEval(TPosicion* pPos, SINT32 s32Eval)
{
	ComputarBB(pPos);
	pPos->s32EvalAmenaza = DeterminarAmenaza(pPos);
	pPos->s32Eval = s32Eval;
}

/*
 *
 * Jugada
 *
 */

//
// Get (PPPP DDDD DDHH HHHH)
//
static INLINE SINT32 Jug_GetVal(TJugada jug)
{
	return(jug.s32Val);
}
static INLINE UINT32 Jug_GetDesde(TJugada jug)
{
	assert(((jug.u32Mov >> 6) & 0x003F) < 64);
	return((jug.u32Mov >> 6) & 0x003F);
}
static INLINE UINT32 Jug_GetHasta(TJugada jug)
{
	assert((jug.u32Mov & 0x003F) < 64);
	return(jug.u32Mov & 0x003F);
}
static INLINE UINT32 Jug_GetPromo(TJugada jug)
{
	return(jug.u32Mov >> 12);
}
static INLINE UINT32 Jug_GetMov(TJugada jug)
{
	assert(Jug_GetDesde(jug) < 64 && Jug_GetHasta(jug) < 64 && Jug_GetPromo(jug) < 15);
	return(jug.u32Mov);
}
static INLINE BOOL Jug_GetEsNula(TJugada jug)
{
	assert(Jug_GetDesde(jug) < 64 && Jug_GetHasta(jug) < 64 && Jug_GetPromo(jug) < 15);
	return(jug.u32Mov == 0);
}
static INLINE UINT32 Jug_GetDesdeMov(UINT32 u32Mov)
{
	assert(((u32Mov >> 6) & 0x003F) < 64);
	return((u32Mov >> 6) & 0x003F);
}
static INLINE UINT32 Jug_GetHastaMov(UINT32 u32Mov)
{
	assert((u32Mov & 0x003F) < 64);
	return(u32Mov & 0x003F);
}
//
// Set
//
static INLINE void Jug_SetVal(TJugada * pJug, SINT32 s32Val)
{
	pJug->s32Val = s32Val;
	assert(Jug_GetVal(*pJug) == s32Val);
}
static INLINE void Jug_SetMov(TJugada * pJug, UINT32 u32Mov)
{
	pJug->u32Mov = u32Mov;
	assert(Jug_GetMov(*pJug) == u32Mov);
}
static INLINE void Jug_SetDesde(TJugada * pJug,UINT32 u32Casilla)
{
	assert(!(u32Casilla & 0xFFC0));
	pJug->u32Mov &= 0xF03F;
	pJug->u32Mov |= (u32Casilla << 6);
	assert(Jug_GetDesde(*pJug) == u32Casilla);
}
static INLINE void Jug_SetHasta(TJugada * pJug,UINT32 u32Casilla)
{
	assert(!(u32Casilla & 0xFFC0));
	pJug->u32Mov &= 0xFFC0;
	pJug->u32Mov |= u32Casilla;
	assert(Jug_GetHasta(*pJug) == u32Casilla);
}
static INLINE void Jug_SetPromo(TJugada * pJug,UINT32 u32Pieza)
{
	pJug->u32Mov &= 0x0FFF;
	pJug->u32Mov |= (u32Pieza << 12);
	assert(Jug_GetPromo(*pJug) == u32Pieza);
}
static INLINE void Jug_SetMovEvalCero(TJugada * pJug,UINT32 u32Desde,UINT32 u32Hasta,UINT32 u32Promo)
{
	assert(!(u32Desde & 0xFFC0));
	assert(!(u32Hasta & 0xFFC0));
	pJug->u32Mov = ((u32Desde << 6) | u32Hasta | (u32Promo << 12));
	assert(Jug_GetDesde(*pJug) == u32Desde);
	assert(Jug_GetHasta(*pJug) == u32Hasta);
	assert(Jug_GetPromo(*pJug) == u32Promo);
	pJug->s32Val = 0;
}
static INLINE void Jug_SetMovConEval(TJugada * pJug,UINT32 u32Desde,UINT32 u32Hasta,UINT32 u32Promo,SINT32 s32Eval)
{
	assert(!(u32Desde & 0xFFC0));
	assert(!(u32Hasta & 0xFFC0));
	pJug->u32Mov = ((u32Desde << 6) | u32Hasta | (u32Promo << 12));
	assert(Jug_GetDesde(*pJug) == u32Desde);
	assert(Jug_GetHasta(*pJug) == u32Hasta);
	assert(Jug_GetPromo(*pJug) == u32Promo);
	pJug->s32Val = s32Eval;
}
static INLINE void Jug_SetMovConHistoria(TJugada* pJug, UINT32 u32Desde, UINT32 u32Hasta, UINT32 u32Promo)
{
	assert(!(u32Desde & 0xFFC0));
	assert(!(u32Hasta & 0xFFC0));
	pJug->u32Mov = ((u32Desde << 6) | u32Hasta | (u32Promo << 12));
	assert(Jug_GetDesde(*pJug) == u32Desde);
	assert(Jug_GetHasta(*pJug) == u32Hasta);
	assert(Jug_GetPromo(*pJug) == u32Promo);
	pJug->s32Val = as32Historia[u32Desde][u32Hasta];
}
static INLINE void Jug_SetNula(TJugada* pJug)
{
	pJug->u32Mov = 0;
	pJug->s32Val = 0;
}

/*
 *
 * Pieza
 *
 */

//
// Get
//
static INLINE UINT32 Pie_GetEsPasadoB(TPosicion * pPos,UINT32 u32Casilla)
{
	return((au64AreaPasadoB[u32Casilla] & pPos->u64PeonesN) == BB_TABLEROVACIO);
}
static INLINE UINT32 Pie_GetEsPasadoN(TPosicion * pPos,UINT32 u32Casilla)
{
	return((au64AreaPasadoN[u32Casilla] & pPos->u64PeonesB) == BB_TABLEROVACIO);
}

/*
 *
 * Tablero
 *
 */
static INLINE UINT32 Tab_GetColumna(UINT32 u32Casilla)
{
	assert(u32Casilla < 64);
	return(u32Casilla & 7);
}
static INLINE UINT32 Tab_GetFila(UINT32 u32Casilla)
{
	assert(u32Casilla < 64);
	return(u32Casilla >> 3);
}
static INLINE UINT32 Tab_GetDistancia(UINT32 u32Cas1, UINT32 u32Cas2)
{
	assert(u32Cas1 < 64);
	assert(u32Cas2 < 64);
	return(max(abs(Tab_GetFila(u32Cas1) - Tab_GetFila(u32Cas2)), abs(Tab_GetColumna(u32Cas1) - Tab_GetColumna(u32Cas2))));
}

/*
 *
 * Ataques
 *
 */

/*
	+-----------------------------------------------------------------------+
	|																		|
	|	Estas funciones determinan ataques en todas direcciones, sin		|
	|	discriminar si la pieza en que acaba la fila o columna es propia	|
	|	o ajena, es decir, que estas funciones rellenan bitmaps hasta lo	|
	|	primero que se encuentren inclusive. Por tanto, si se quiere usar	|
	|	para generación de movimientos, habrá posteriormente que quitar		|
	|	las piezas propias del bitmap										|
	|																		|
	+-----------------------------------------------------------------------+
*/

//
// Ataques verticales
//
static INLINE UINT64 AtaquesArriba(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Pieza |= (u64Pieza << 8) & u64Vacias;
	u64Vacias &= (u64Vacias <<  8);
	u64Pieza |= (u64Pieza << 16) & u64Vacias;
	u64Vacias &= (u64Vacias << 16);
	u64Pieza |= (u64Pieza << 32) & u64Vacias;
	return(u64Pieza << 8);
}
static INLINE UINT64 RellenarArriba(UINT64 u64)
{
	u64 |= (u64 << 8);
	u64 |= (u64 << 16);
	u64 |= (u64 << 32);
	return(u64 << 8);
}
static INLINE UINT64 AtaquesAbajo(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Pieza |= (u64Pieza >>  8) & u64Vacias;
	u64Vacias &= (u64Vacias >>  8);
	u64Pieza |= (u64Pieza >> 16) & u64Vacias;
	u64Vacias &= (u64Vacias >> 16);
	u64Pieza |= (u64Pieza >> 32) & u64Vacias;
	return(u64Pieza >> 8);
}
static INLINE UINT64 RellenarAbajo(UINT64 u64)
{
	u64 |= (u64 >> 8);
	u64 |= (u64 >> 16);
	u64 |= (u64 >> 32);
	return(u64 >> 8);
}
//
// Ataques horizontales
//
static INLINE UINT64 AtaquesDerecha(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Vacias &= BB_SINCOLIZDA;
	u64Pieza |= (u64Pieza >> 1) & u64Vacias;
	u64Vacias &= (u64Vacias >> 1);
	u64Pieza |= (u64Pieza >> 2) & u64Vacias;
	u64Vacias &= (u64Vacias >> 2);
	u64Pieza |= (u64Pieza >> 4) & u64Vacias;
	u64Vacias &= (u64Vacias >> 4);
	return((u64Pieza >> 1) & BB_SINCOLIZDA);
}
static INLINE UINT64 AtaquesIzquierda(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Vacias &= BB_SINCOLDCHA;
	u64Pieza |= (u64Pieza << 1) & u64Vacias;
	u64Vacias &= (u64Vacias << 1);
	u64Pieza |= (u64Pieza << 2) & u64Vacias;
	u64Vacias &= (u64Vacias << 2);
	u64Pieza |= (u64Pieza << 4) & u64Vacias;
	u64Vacias &= (u64Vacias << 4);
	return((u64Pieza << 1) & BB_SINCOLDCHA);
}
//
// Ataques Torre
//
static INLINE UINT64 AtaquesTorre(UINT64 u64Pieza, UINT64 u64Vacias)
{
	return(AtaquesArriba(u64Pieza, u64Vacias)
		 | AtaquesAbajo(u64Pieza, u64Vacias)
		 | AtaquesIzquierda(u64Pieza, u64Vacias)
		 | AtaquesDerecha(u64Pieza, u64Vacias));
}
//
// Ataques peón
//
static INLINE UINT64 AtaquesPeonB_old(UINT64 u64Pieza)
{
	UINT64 u64 = u64Pieza;
	u64Pieza ^= u64;
	u64Pieza |= (u64 << 7) & BB_SINCOLIZDA;
	u64Pieza |= (u64 << 9) & BB_SINCOLDCHA;
	return(u64Pieza);
}
static INLINE UINT64 AtaquesPeonB(UINT64 u64Pieza)
{
#if defined(_DEBUG)
	UINT64 old = AtaquesPeonB_old(u64Pieza);
	UINT64 new = ((u64Pieza << 7) & BB_SINCOLIZDA) | ((u64Pieza << 9) & BB_SINCOLDCHA);
	assert(old == new);
#endif
	return ((u64Pieza << 7) & BB_SINCOLIZDA) | ((u64Pieza << 9) & BB_SINCOLDCHA);
}
static INLINE UINT64 AtaquesPeonN(UINT64 u64Pieza)
{
	UINT64 u64 = u64Pieza;
	u64Pieza ^= u64;
	u64Pieza |= (u64 >> 7) & BB_SINCOLDCHA;
	u64Pieza |= (u64 >> 9) & BB_SINCOLIZDA;
	return(u64Pieza);
}
//
// Ataques caballo
//
static INLINE UINT64 AtaquesCaballo(UINT64 u64Pieza)
{
	UINT64 u64 = u64Pieza;
	u64Pieza ^= u64;
	u64Pieza |= (u64 >> 17) & BB_SINCOLIZDA;
	u64Pieza |= (u64 << 15) & BB_SINCOLIZDA;
	u64Pieza |= (u64 >> 15) & BB_SINCOLDCHA;
	u64Pieza |= (u64 << 17) & BB_SINCOLDCHA;
	u64Pieza |= (u64 >> 10) & BB_SIN2COLSIZDA;
	u64Pieza |= (u64 <<  6) & BB_SIN2COLSIZDA;
	u64Pieza |= (u64 >>  6) & BB_SIN2COLSDCHA;
	u64Pieza |= (u64 << 10) & BB_SIN2COLSDCHA;
	return(u64Pieza);
}
//
// Ataques rey
//
static INLINE UINT64 AtaquesRey(UINT64 u64Pieza)
{
	UINT64 u64 = u64Pieza;

	u64Pieza |= (u64Pieza >> 1) & BB_SINCOLIZDA;
	u64Pieza |= (u64Pieza << 1) & BB_SINCOLDCHA;
	u64Pieza |=  u64Pieza << 8;
	u64Pieza |=  u64Pieza >> 8;
	return(u64Pieza ^ u64);
}
//
// Ataques diagonales
//
static INLINE UINT64 AtaquesAbajoDcha(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Vacias &= BB_SINCOLIZDA;
	u64Pieza |= (u64Pieza >>  9) & u64Vacias;
	u64Vacias &= (u64Vacias >>  9);
	u64Pieza |= (u64Pieza >> 18) & u64Vacias;
	u64Vacias &= (u64Vacias >> 18);
	u64Pieza |= (u64Pieza >> 27) & u64Vacias;
	u64Vacias &= (u64Vacias >> 27);
	return((u64Pieza >> 9) & BB_SINCOLIZDA);
}
static INLINE UINT64 AtaquesArribaDcha(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Vacias &= BB_SINCOLIZDA;
	u64Pieza |= (u64Pieza <<  7) & u64Vacias;
	u64Vacias &= (u64Vacias <<  7);
	u64Pieza |= (u64Pieza << 14) & u64Vacias;
	u64Vacias &= (u64Vacias << 14);
	u64Pieza |= (u64Pieza << 21) & u64Vacias;
	u64Vacias &= (u64Vacias << 21);
	return((u64Pieza << 7) & BB_SINCOLIZDA);
}
static INLINE UINT64 AtaquesAbajoIzda(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Vacias &= BB_SINCOLDCHA;
	u64Pieza |= (u64Pieza >>  7) & u64Vacias;
	u64Vacias &= (u64Vacias >>  7);
	u64Pieza |= (u64Pieza >> 14) & u64Vacias;
	u64Vacias &= (u64Vacias >> 14);
	u64Pieza |= (u64Pieza >> 21) & u64Vacias;
	u64Vacias &= (u64Vacias >> 21);
	return((u64Pieza >> 7) & BB_SINCOLDCHA);
}
static INLINE UINT64 AtaquesArribaIzda(UINT64 u64Pieza, UINT64 u64Vacias)
{
	u64Vacias &= BB_SINCOLDCHA;
	u64Pieza |= (u64Pieza <<  9) & u64Vacias;
	u64Vacias &= (u64Vacias <<  9);
	u64Pieza |= (u64Pieza << 18) & u64Vacias;
	u64Vacias &= (u64Vacias << 18);
	u64Pieza |= (u64Pieza << 27) & u64Vacias;
	u64Vacias &= (u64Vacias << 27);
	return((u64Pieza << 9) & BB_SINCOLDCHA);
}
//
// Ataques alfil
//
static INLINE UINT64 AtaquesAlfil(UINT64 u64Pieza, UINT64 u64Vacias)
{
	return(AtaquesArribaIzda(u64Pieza,u64Vacias)
		 | AtaquesArribaDcha(u64Pieza,u64Vacias)
		 | AtaquesAbajoIzda(u64Pieza,u64Vacias)
		 | AtaquesAbajoDcha(u64Pieza,u64Vacias));
}

static inline void CambiarJugada(TPosicion* pPos, TJugada* pJug)
{
	TJugada* pJug2;
	TJugada	jug;

	for (pJug2 = pJug - 1; pJug2 >= pPos->pListaJug; pJug2--)
	{
		if (Jug_GetVal(*pJug2) > Jug_GetVal(*pJug))
		{
			jug = *pJug;
			*pJug = *pJug2;
			*pJug2 = jug;
		}
	}
}

/*
 * Probe the Win-Draw-Loss (WDL) table.
 *
 * PARAMETERS:
 * - white, black, kings, queens, rooks, bishops, knights, pawns:
 *   The current position (bitboards).
 * - rule50:
 *   The 50-move half-move clock.
 * - castling:
 *   Castling rights.  Set to zero if no castling is possible.
 * - ep:
 *   The en passant square (if exists).  Set to zero if there is no en passant
 *   square.
 * - turn:
 *   true=white, false=black
 *
 * RETURN:
 * - One of {TB_LOSS, TB_BLESSED_LOSS, TB_DRAW, TB_CURSED_WIN, TB_WIN}.
 *   Otherwise returns TB_RESULT_FAILED if the probe failed.
 *
 * NOTES:
 * - Engines should use this function during search.
 * - This function is thread safe assuming TB_NO_THREADS is disabled.
 */

static inline unsigned tb_probe_wdl(uint64_t _white, uint64_t _black, uint64_t _kings, uint64_t _queens, uint64_t _rooks, uint64_t _bishops, uint64_t _knights,	uint64_t _pawns,
									unsigned _rule50, unsigned _castling, unsigned _ep,	bool _turn)
{
	if (_castling != 0)
		return TB_RESULT_FAILED;
	if (_rule50 != 0)
		return TB_RESULT_FAILED;
	return tb_probe_wdl_impl(_white, _black, _kings, _queens, _rooks, _bishops, _knights, _pawns, _ep, _turn);
}

/*
 * Probe the Distance-To-Zero (DTZ) table.
 *
 * PARAMETERS:
 * - white, black, kings, queens, rooks, bishops, knights, pawns:
 *   The current position (bitboards).
 * - rule50:
 *   The 50-move half-move clock.
 * - castling:
 *   Castling rights.  Set to zero if no castling is possible.
 * - ep:
 *   The en passant square (if exists).  Set to zero if there is no en passant
 *   square.
 * - turn:
 *   true=white, false=black
 * - results (OPTIONAL):
 *   Alternative results, one for each possible legal move.  The passed array
 *   must be TB_MAX_MOVES in size.
 *   If alternative results are not desired then set results=NULL.
 *
 * RETURN:
 * - A TB_RESULT value comprising:
 *   1) The WDL value (TB_GET_WDL)
 *   2) The suggested move (TB_GET_FROM, TB_GET_TO, TB_GET_PROMOTES, TB_GET_EP)
 *   3) The DTZ value (TB_GET_DTZ)
 *   The suggested move is guaranteed to preserved the WDL value.
 *
 *   Otherwise:
 *   1) TB_RESULT_STALEMATE is returned if the position is in stalemate.
 *   2) TB_RESULT_CHECKMATE is returned if the position is in checkmate.
 *   3) TB_RESULT_FAILED is returned if the probe failed.
 *
 *   If results!=NULL, then a TB_RESULT for each legal move will be generated
 *   and stored in the results array.  The results array will be terminated
 *   by TB_RESULT_FAILED.
 *
 * NOTES:
 * - Engines can use this function to probe at the root.  This function should
 *   not be used during search.
 * - DTZ tablebases can suggest unnatural moves, especially for losing
 *   positions.  Engines may prefer to traditional search combined with WDL
 *   move filtering using the alternative results array.
 * - This function is NOT thread safe.  For engines this function should only
 *   be called once at the root per search.
 */
static inline unsigned tb_probe_root(uint64_t _white, uint64_t _black, uint64_t _kings, uint64_t _queens, uint64_t _rooks, uint64_t _bishops, uint64_t _knights, uint64_t _pawns,
									unsigned _rule50, unsigned _castling, unsigned _ep, bool _turn, unsigned* _results)
{
	if (_castling != 0)
		return TB_RESULT_FAILED;
	return tb_probe_root_impl(_white, _black, _kings, _queens, _rooks, _bishops, _knights, _pawns, _rule50, _ep, _turn, _results);
}

static inline void Bus_Historia_Sumar(TJugada jug, SINT32 s32Prof)
{
	static const int MAX_HISTORIA = 0x0FFFFF;
	const int desde = Jug_GetDesde(jug);
	const int hasta = Jug_GetHasta(jug);

	as32Historia[desde][hasta] += (s32Prof * s32Prof);

	// No debería llegarse nunca, pero por si acaso...
	if (as32Historia[desde][hasta] > MAX_HISTORIA)
		for (int d = 0; d < 64; d++)
			for (int h = 0; h < 64; h++)
				as32Historia[d][h] /= 8;
}
static inline void Bus_Historia_Restar(TJugada jug, SINT32 s32Prof)
{
	const int desde = Jug_GetDesde(jug);
	const int hasta = Jug_GetHasta(jug);

	as32Historia[desde][hasta] -= ((s32Prof * s32Prof) / 2);
}
static inline UINT32 Bus_Historia_Get_Val(TJugada jug)
{
	return (as32Historia[Jug_GetDesde(jug)][Jug_GetHasta(jug)]);
}
#if defined(REFUTACION)
static inline void Bus_Refuta_Grabar(TPosicion* pPos, TJugada jug)
{
	const UINT8 pieza1 = (pPos - 1)->UltMovPieza;	// Pieza movida hace 1 movimiento
	const UINT8 hasta1 = (pPos - 1)->UltMovHasta;
	const UINT8 pieza2 = (pPos - 2)->UltMovPieza;	// Pieza movida hace 2 movimientos
	const UINT8 hasta2 = (pPos - 2)->UltMovHasta;

	au32Refuta[pieza2][hasta2][pieza1][hasta1] = Jug_GetMov(jug);
}

static inline TJugada Bus_Refuta_Get(TPosicion* pPos)
{
	const UINT8 pieza1 = (pPos - 1)->UltMovPieza;	// Pieza movida hace 1 movimiento
	const UINT8 hasta1 = (pPos - 1)->UltMovHasta;
	const UINT8 pieza2 = (pPos - 2)->UltMovPieza;	// Pieza movida hace 2 movimientos
	const UINT8 hasta2 = (pPos - 2)->UltMovHasta;
	TJugada jug;

	if (pieza1 && pieza2)
		jug.u32Mov = au32Refuta[pieza2][hasta2][pieza1][hasta1];
	else
		jug.u32Mov = 0;
	return(jug);
}
#endif