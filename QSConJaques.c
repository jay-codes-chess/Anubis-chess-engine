
#include "Preprocesador.h"
#include "Tipos.h"
#include "Funciones.h"
#include "Variables.h"
#include "Inline.h"
#include "Bitboards_inline.h"

/*
 *
 * LocalizarJugHash
 *
 * Recibe: Punteros a la primera y última jugadas a revisar, Jugada hash
 *
 */
static void LocalizarJugHash(TJugada * pJug1, TJugada * pJug2, TJugada jugHash)
{
	if (Jug_GetMov(jugHash))
	{
		for (; pJug1 < pJug2; pJug1++)
		{
			if (Jug_GetMov(*pJug1) == Jug_GetMov(jugHash))
			{
				Jug_SetVal(pJug1, 10000);
				return;
			}
		}
	}
}

/*
 *
 * QSConJaques
 *
 *
 * Recibe: Puntero a la posición actual, alfa y beta, ply	actual, profundidad restante
 *
 * Devuelve: El valor QSearch del nodo analizado
 *
 */
SINT32 QSConJaques(TPosicion * pPos, SINT32 s32Alfa, SINT32 s32Beta, SINT32 s32Ply, SINT32 s32Prof)
{
	TJugada		jugMejor = JUGADA_NULA;
	TJugada *	pJug;
	TJugada *	pJugTmp;
	UINT32		u32Legales;
	UINT32		u32ContadorJug = 0;
	SINT32		s32Val;
	SINT32		s32Mejor;	// Para fail soft
	BOOL		bComprobarTodo;
	SINT32		s32AlfaOriginal = s32Alfa;

	// Profundidad selectiva
	if (s32Ply > dbDatosBusqueda.s32ProfSel)
		dbDatosBusqueda.s32ProfSel = s32Ply;

	//
	// Salida por indicación externa
	//
	if (GetEsHoraDeParar(&g_tReloj, &dbDatosBusqueda, as32PilaEvaluaciones))
	{
		dbDatosBusqueda.bAbortar = TRUE;
		return(TABLAS);
	}
	if (ProcesarComando())
	{
		dbDatosBusqueda.bAbortar = TRUE;
		return(TABLAS);
	}
	if (pPos->u8Cincuenta > 100)
		return(TABLAS);
	if (SegundaRepeticion(pPos))
		return(TABLAS);
	if (InsuficienteMaterial(pPos))
		return(TABLAS);
	if (s32Ply > MAX_PLIES)
		return(TABLAS);

	//
	// Poda de distancia a mate
	//
	{
		s32Alfa = max(-INFINITO + s32Ply, s32Alfa);
		s32Beta = min(INFINITO - s32Ply - 1, s32Beta);

		if (s32Alfa >= s32Beta)
			return(s32Alfa);
	}

	//
	// Consultar hash general
	//
	TNodoHash* pNodoHash = NULL;
	TJugada jugHash = JUGADA_NULA;
	SINT32 s32EvalHash = -INFINITO;
	int iBoundHash = 0;
	if (ConsultarHash(pPos, 0, s32Alfa, s32Beta, &pNodoHash))
	{
		if (pPos->u8Cincuenta < 90 && !Bus_DoyMatePronto(abs(pNodoHash->jug.s32Val)))
			return(EvalFromTT(pNodoHash->jug.s32Val, s32Ply));
	}
	if (pNodoHash != NULL)
	{
		jugHash = pNodoHash->jug;
		s32EvalHash = EvalFromTT(pNodoHash->jug.s32Val, s32Ply);
		iBoundHash = Hash_GetBound(pNodoHash);
	}

	//
	// Ver si me puedo ahorrar una llamada a eval
	//
	if (Pos_GetJaqueado(pPos))
		Pos_SetEval(pPos, NO_EVAL);
	else
	{
		if ((iBoundHash == BOUND_EXACTO)
			|| (iBoundHash == BOUND_UPPER && s32EvalHash < s32Alfa)
			|| (iBoundHash == BOUND_LOWER && s32EvalHash > s32Beta))
		{
			Pos_SetEval(pPos, s32EvalHash);
		}
		else
			pPos->s32Eval = Evaluar(pPos);
	}

	//
	// Salida por profundidad agotada
	//
	if (s32Ply >= MAX_PLIES - 1)
		return(pPos->s32Eval);

	//
	// Fin de profundidad de jaques
	//
	if (s32Prof <= 0)
	{
		if (Pos_GetJaqueado(pPos) && pPos->u32Fase != FAS_FINAL_DAMAS && pPos->u32Fase != FAS_FINAL_PESADAS)
			s32Prof = 0;
		else
			return(QSearch(pPos, s32Alfa, s32Beta, s32Ply));
	}

	//
	// Determinar si hay que comprobarlo todo o hacer una búsqueda normal de capturas y jaques
	//
	if (Pos_GetJaqueado(pPos))
		bComprobarTodo = TRUE;
	else
		bComprobarTodo = FALSE;

	//
	// Funcionamiento diferente según estemos en jaque o no
	//
	if (bComprobarTodo)
	{
		// No hay salida por "stand-pat" pq tenemos, por fuerza, que analizar todos los escapes
		s32Mejor = -INFINITO + s32Ply;

		//
		// Generar jugadas: las genero todas (capturas y no capturas) para saber cuántas legales hay (en jaque, sólo genero jugadas legales)
		//  pJugTmp me sirve para saber dónde empiezan las capturas, y aplicar ordenación SEE
		//
		pJugTmp = GenerarMovimientos(pPos, pPos->pListaJug);
		pJug = GenerarCapturas(pPos, pJugTmp + 1, FALSE);
		if (pJug < pPos->pListaJug)
			u32Legales = 0;
		else
		{
			u32Legales = (UINT32)(pJug - pPos->pListaJug) + 1;
			#if defined(QS_COMPROBAR_PROGRESO_JAQUES)
				//
				// Si llevamos un rato dando jaques sin progresar, pasamos
				//
				if (Pos_GetJaqueado(pPos) && Pos_GetJaqueado(pPos-2) && Pos_GetJaqueado(pPos-4) && Pos_GetJaqueado(pPos-6) && abs(pPos->s32Eval - (pPos-6)->s32Eval) < 20)
					return(QSearch(pPos,s32Alfa,s32Beta,s32Ply));
				if (Pos_GetJaqueado(pPos) && Pos_GetJaqueado(pPos-2) && Pos_GetJaqueado(pPos-4) && abs(pPos->s32Eval - (pPos-4)->s32Eval) < 15)
					return(QSearch(pPos,s32Alfa,s32Beta,s32Ply));
			#endif
		}

		if (u32Legales < 3)
		{
			if (u32Legales < 2)
			{
				if (u32Legales)	// un solo escape
				{
					#if defined(PER_EXTDOBLE1SOLOESCAPE)
						s32Prof += 2;
					#else
						s32Prof++;
					#endif
				}
				else  // ningún escape
				{
					if (Pos_GetJaqueado(pPos))
						return(-INFINITO + s32Ply);
					else
						return(TABLAS);
				}
			}
			else // dos escapes
				s32Prof++;
		}
		else if (s32Prof == 0)
			return(QSearch(pPos, s32Alfa, s32Beta, s32Ply));

		//
		// Bucle principal de "comprobar todo"
		//
		if (pJug >= pPos->pListaJug)
		{
			// Asignar valor SEE a las capturas para ordenación
			AsignarValorSEE(pPos, pJugTmp + 1, pJug);
			LocalizarJugHash(pPos->pListaJug, pJug, jugHash);
			for (; pJug >= pPos->pListaJug; pJug--)
			{
				CambiarJugada(pPos, pJug);

				//
				// Mover y analizar
				//
				assert(JugadaCorrecta(pPos, *pJug));
				// Atención: puedo haber generado una jugada ilegal, si el rey está bajo ataque "deslizante" y retrocede por la misma fila
				if (!Mover(pPos, pJug))
				{
					u32Legales--;
					if (!u32Legales)
					{
						// Es posible estar aquí no estando en jaque, si el rey no tiene casillas. Si además no tengo jugadas legales disponibles, estoy ahogado
						if (Pos_GetJaqueado(pPos))
							return(-INFINITO + s32Ply);
						else
							return(TABLAS);
					}
					continue;
				}
				u32ContadorJug++;
				dbDatosBusqueda.u32NumNodosQJ++;

				s32Val = -QSConJaques(pPos + 1, -s32Beta, -s32Alfa, s32Ply + 1, s32Prof - 1);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				//
				// Salir si beta-cutoff. Reajustar Alfa y Mejor
				//
				if (s32Val > s32Mejor)
				{
					jugMejor = *pJug;
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, 0, EvalToTT(s32Val, s32Ply), jugHash, s32Alfa, s32Beta);
							return(s32Val);
						}
						s32Alfa = s32Val;
					}
					s32Mejor = s32Val;
				}
			}
		}
	}
	else // if (bComprobarTodo)
	{
		//
		// Salida por "stand-pat"
		//
		s32Mejor = pPos->s32Eval;
		#if defined(QS_STANDPAT_AUM)
			s32Mejor += QS_STANDPAT_AUM;
		#endif

		if (s32Mejor > s32Alfa)
		{
			if (s32Mejor >= s32Beta)
			{
				// OJO: ¿tiene sentido grabar en la tabla hash cuando estamos saliendo por standpat?
				GrabarHash(pPos, 0, EvalToTT(s32Mejor, s32Ply), jugHash, s32Alfa, s32Beta);
				return(s32Mejor);
			}
			s32Alfa = s32Mejor;
		}

		//
		// Bucle principal
		// Capturas
		//
		u32Legales = 0;
		pJug = GenerarCapturas(pPos, pPos->pListaJug, FALSE);
		if (pJug >= pPos->pListaJug)
		{
			// Asignar valor SEE a las capturas para ordenación
			//	Voy a podar en cuanto encuentre una jugada perdedora, así que necesito la lista completamente ordenada, y luego no hago "cambiar"
			AsignarValorSEE(pPos, pPos->pListaJug, pJug);
			LocalizarJugHash(pPos->pListaJug, pJug, jugHash);
			Ordenar(pPos, pJug);
			for (; pJug >= pPos->pListaJug; pJug--)
			{
				// En caso de querer podar, por SEE o por futility, necesito comprobar que la jugada no jaquea, pues sería un sacrificio que
				//	quiero estudiar. Por tanto, muevo antes que nada para saber si hay o no jaque a la siguiente
				if (!Mover(pPos,pJug))
					continue;

				// Si la jugada pierde material (según SEE), no me molesto en analizarla, salvo que sea jaque
				#if defined(QS_PODA_SEE_NEGATIVO)
					if (Jug_GetVal(*pJug) <= -VAL_PEON
						&& !Bus_ReciboMatePronto(s32Mejor)
						&& !Pos_GetJaqueado(pPos + 1))
					{
						continue;
					}
				#endif

				//
				// Poda de futility en qsearch
				// Salvo que sea jaque
				//
				#if defined(QS_PODA_FUTIL)
					if (pPos->u32Fase < FAS_FINAL
						&& pPos->s32Eval + Jug_GetVal(*pJug) + QS_PODA_FUTIL < s32Alfa
						&& !Pos_GetJaqueado(pPos + 1))
					{
						continue;
					}
				#endif

				//
				// Analizar
				//
				u32Legales++;
				u32ContadorJug++;
				dbDatosBusqueda.u32NumNodosQJ++;

				s32Val = -QSConJaques(pPos + 1, -s32Beta, -s32Alfa, s32Ply + 1, s32Prof - 1);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				//
				// Salir si beta-cutoff. Reajustar Alfa y Mejor
				//
				if (s32Val > s32Mejor)
				{
					jugMejor = *pJug;
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, 0, EvalToTT(s32Val, s32Ply), jugMejor, s32Alfa, s32Beta);
							return(s32Val);
						}
						s32Alfa = s32Val;
					}
					s32Mejor = s32Val;
				}
			}
		} // if (pJug >= pPos->pListaJug)

		//
		// Bucle principal
		// Jaques
		//
		pJug = GenerarJaquesNoCapturas(pPos, pPos->pListaJug);
		if (pJug >= pPos->pListaJug)
		{
			// No tenemos ni idea del orden en que deben ser analizados los jaques
			//	Vamos a intentar colocar la jugada de la tabla hash aquí
			//	Calculo el SEE y omito los jaques que pierden material
			#if defined(PER_PODARJAQUESSEENEGATIVO)
				AsignarValorSEE(pPos, pPos->pListaJug, pJug);
			#endif

			LocalizarJugHash(pPos->pListaJug, pJug, jugHash);
			Ordenar(pPos, pJug);
			for (; pJug >= pPos->pListaJug; pJug--)
			{
				// Podar jaques que pierden material
				#if defined(PER_PODARJAQUESSEENEGATIVO)
					if (Jug_GetVal(*pJug) <= -VAL_PEON && !Bus_ReciboMatePronto(s32Mejor))
						break;
				#endif

				//
				// Poda de futility en qsearch
				// Salvo que sea jaque
				//
				#if defined(QS_PODA_FUTIL)
					if (pPos->u32Fase < FAS_FINAL
						&& pPos->s32Eval + Jug_GetVal(*pJug) + QS_PODA_FUTIL < s32Alfa
						&& !Pos_GetJaqueado(pPos + 1))
					{
						break;
					}
				#endif

				//
				// Mover y analizar
				//
				if (!Mover(pPos, pJug))
					continue;

				#if defined(PER_PODARJAQUESPOSPERDIDA)
					if (pPos->s32Eval < s32Alfa - 1000 && pPos->s32Eval < 0)
						continue;
				#endif

				u32Legales++;
				u32ContadorJug++;
				dbDatosBusqueda.u32NumNodosQJ++;
				s32Val = -QSConJaques(pPos + 1, -s32Beta, -s32Alfa, s32Ply + 1, s32Prof - 1);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				//
				// Salir si beta-cutoff. Reajustar Alfa y Mejor
				//
				if (s32Val > s32Mejor)
				{
					jugMejor = *pJug;
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, 0, EvalToTT(s32Val, s32Ply), jugMejor, s32Alfa, s32Beta);
							return(s32Val);
						}
						s32Alfa = s32Val;
					}
					s32Mejor = s32Val;
				}
			}
		} // if (pJug >= pPos->pListaJug)
	} // else [if (Pos_GetJaqueado(pPos))]

	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);

	// No hemos cortado en beta
	GrabarHash(pPos, 0, EvalToTT(s32Mejor, s32Ply), jugMejor, s32AlfaOriginal, s32Beta);

	if (s32Mejor > s32AlfaOriginal && !Jug_GetEsNula(jugMejor))
	{
		if (s32Mejor < s32Beta)
			Bus_ActualizarPV(jugMejor, s32Ply);
	}
	return(s32Mejor);
}