
#include "Preprocesador.h"
#include "Tipos.h"
#include "Inline.h"
#include "Funciones.h"
#include "Bitboards_inline.h"
#include "nnue.h"

static BOOL Mejorando(TPosicion* pPos, SINT32 s32Ply)
{
	if (dbDatosBusqueda.u32NumPlyPartida + s32Ply < 4)
		return(FALSE);

	if (pPos->s32Eval == NO_EVAL)
		return(FALSE);

	if (pPos->u8Jaqueado)
		return(FALSE);
	else
	{
		if ((pPos - 2)->u8Jaqueado)
		{
			if ((pPos - 4)->u8Jaqueado)
				return(TRUE);
			else if (pPos->s32Eval > (pPos - 4)->s32Eval)
				return(TRUE);
		}
		else
		{
			if (pPos->s32Eval > (pPos - 2)->s32Eval)
				return(TRUE);
		}
	}
	return(FALSE);
}

/*
 * ActualizarKillers
 *
 * Recibe: Una jugada y el ply actual
 *
 * Descripción: Actualiza los killers para el ply indicado con la jugada pasada. Sólo actualizo si la jugada es
 *  distinta del primero, para evitar tener los dos killers iguales. No necesito verificar que no es una captura
 *  ganadora porque sólo llamo a esta función desde el apartado de "resto + capt. perdedoras"
 *
 */
static void ActualizarKillers(TJugada jug, SINT32 s32Ply)
{
	if (Jug_GetMov(jug) != Jug_GetMov(ajugKillers[s32Ply][0]))
	{
		ajugKillers[s32Ply][1] = ajugKillers[s32Ply][0];
		ajugKillers[s32Ply][0] = jug;
	}
}

/*
 * LocalizarJugadasPrometedoras
 *
 * Recibe: Punteros a la primera y última jugadas a revisar, ply actual
 *
 * Descripción: Revisa la lista de jugadas en busca de los killers del presente ply. Cada vez que encuentra uno,
 *  le pone un valor para que luego Cambiar() lo lleve a su orden correspondiente
 *  - Idea de ES: Comprobar tb los killers de dos niveles anteriores
 *  - Aparte de los killers, trato también de detectar los escapes, es decir, jugadas que sacan una pieza de una
 *    casilla atacada y la llevan a una	no atacada
 *  - Utilizo también, si es posible, los killers de 2 plies más profundo
 *
 * OJO: ¿por qué el primer killer lo ordeno con más prioridad? ¿no debería dar igual?
 * OJO: los killers de 2 niveles más profundo no los he comprobado, no sé si existen o son posiciones de memoria
 *      con basura, así que creo que no debería darles tanta prioridad
 *
 */
static void LocalizarJugadasPrometedoras(TPosicion * pPos, TJugada * pJug1, TJugada * pJug2, SINT32 s32Ply)
{
	for (; pJug1 < pJug2; pJug1++)
	{
		if (Jug_GetMov(*pJug1) == Jug_GetMov(ajugKillers[s32Ply][0]))
			Jug_SetVal(pJug1, 200000);
		else if (Jug_GetMov(*pJug1) == Jug_GetMov(ajugKillers[s32Ply][1]))
			Jug_SetVal(pJug1, 100000);
		else
		{
			if (s32Ply > 2)
			{
				if (Jug_GetMov(*pJug1) == Jug_GetMov(ajugKillers[s32Ply - 2][0]))
					Jug_SetVal(pJug1, 75000);
				else if (Jug_GetMov(*pJug1) == Jug_GetMov(ajugKillers[s32Ply - 2][1]))
					Jug_SetVal(pJug1, 50000);
			}
			else
			{
				if (Jug_GetMov(*pJug1) == Jug_GetMov(ajugKillers[s32Ply + 2][0]))
					Jug_SetVal(pJug1, 75000);
				else if (Jug_GetMov(*pJug1) == Jug_GetMov(ajugKillers[s32Ply + 2][1]))
					Jug_SetVal(pJug1, 50000);
			}
		}
	}
}

/*
 * 
 * Analizar
 *
 */
static SINT32 Analizar(TPosicion * pPos,
                       SINT32	s32Alfa,
                       SINT32	s32Beta,
                       SINT32	s32Prof,
                       SINT32	s32Extensiones,
                       SINT32	s32Ply,
                       BOOL		bEsPV,
					   BOOL		bEsCutNode)
{
	SINT32 s32Val;

	if (bEsPV)
		s32Val = -AlfaBetaPV(pPos+1, -s32Beta, -s32Alfa, s32Prof-1+s32Extensiones, s32Ply+1);
	else
	{
		s32Val = -AlfaBeta(pPos+1, -(s32Alfa+1), -s32Alfa, s32Prof-1+s32Extensiones, s32Ply+1, bEsCutNode);
		if (dbDatosBusqueda.bAbortar)
			return(TABLAS);
		if (s32Val > s32Alfa && s32Val < s32Beta)
			s32Val = -AlfaBetaPV(pPos+1, -s32Beta, -s32Alfa, s32Prof-1+s32Extensiones, s32Ply+1);
	}
	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);

	// Los cortes por repeticiones durante la búsqueda me están causando muchos problemas con la tabla hash y el fail
    //  soft. Resulta que, al devolver cero, si el valor real de la posición sin tener en cuenta la repetición es muy
	//  superior, produzco fail high en la raiz que luego no puedo demostrar al abrir la ventana hacia arriba
	// OJO: No entiendo lo que digo arriba. Es preciso estudiar esto y entender si es correcto o no
	else if (s32Val == TABLAS)
		s32Val = max(s32Alfa, TABLAS);

	return(s32Val);
}

/*
 * 
 * AlfaBeta
 *
 */
SINT32 AlfaBeta(TPosicion * pPos, SINT32 s32Alfa, SINT32 s32Beta, SINT32 s32Prof, SINT32 s32Ply, BOOL bEsCutNode)
{
	SINT32 s32Extensiones = 0;
	SINT32 s32Val;
	SINT32 s32Mejor = -INFINITO + s32Ply;		// Para fail-soft
	const SINT32 s32AlfaOriginal = s32Alfa;
	UINT32 u32Legales = 0;
	TJugada * pJug;					            // Para la generación de jugadas
	TJugada	jugMejor = JUGADA_NULA;
	BOOL bMejorando;
	BOOL bGrabarHash = TRUE;

	//
	// Verificaciones de consistencia
	//
	assert(s32Alfa >= -INFINITO);
	assert(s32Alfa < s32Beta);
	assert(s32Beta <= INFINITO);
	assert(s32Ply > 0);
    if (s32Prof < 0)
      s32Prof = 0;

	//
	// Comprobaciones de salida anticipada
	//
	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);
	if (pPos->u8Cincuenta > 100)
		return(TABLAS);
	if (SegundaRepeticion(pPos))
		return(TABLAS);
	if (InsuficienteMaterial(pPos))
		return(TABLAS);
	if (s32Ply >= MAX_PLIES - 1)
		return(TABLAS);

	//
	// Leemos el reloj cada 4096 nodos - también pongo aquí el parseo de comandos
	//
	if ((dbDatosBusqueda.u32NumNodos & 0xFFF) == 0xFFF)
	{
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
	}

	//
	// Salida por profundidad agotada
	//
	if (s32Prof <= 0)
	{
		s32Val = QSConJaques(pPos, s32Alfa, s32Beta, s32Ply, QS_PROF_JAQUES);
		return(s32Val);
	}

	//
	// Poda de distancia a mate
	//
	s32Alfa = max(-INFINITO + s32Ply, s32Alfa);
	s32Beta = min(INFINITO - s32Ply - 1, s32Beta);
	if (s32Alfa >= s32Beta)
		return(s32Alfa);

	//
	// Consultar tabla hash
	//
	TNodoHash * pNodoHash = NULL;
	TJugada jugHash = JUGADA_NULA;
	SINT32 s32EvalHash = NO_EVAL;
	SINT32 s32ProfHash = 0;
	int iBoundHash = 0;
	if (pPos->u32MovExclu == 0 && ConsultarHash(pPos, s32Prof, s32Alfa, s32Beta, &pNodoHash))
	{
		if (!Jug_GetEsNula(pNodoHash->jug))
			jugHash = aJugParaIID[s32Ply] = pNodoHash->jug;

		if (pPos->u8Cincuenta < 90 && !Bus_DoyMatePronto(abs(pNodoHash->jug.s32Val)))
			return(EvalFromTT(pNodoHash->jug.s32Val, s32Ply));
	}
	if (pNodoHash != NULL)
	{
		jugHash = pNodoHash->jug;
		s32EvalHash = EvalFromTT(pNodoHash->jug.s32Val, s32Ply);
		s32ProfHash = pNodoHash->u8Prof;
		iBoundHash = Hash_GetBound(pNodoHash);

		//
		// Podas basadas en hash
		//
		#define PODA_HASH_PROF_DIF 10			// Máxima diferencia permitida de profundidad entre acutal y hash

		#define PODA_HASH_BETA_BASE 185			// Diferencia base contra beta
		#define PODA_HASH_BETA_MULT_DIF 0		// Multiplicador por cada diferencia de profundidad adicional en beta
		#define PODA_HASH_BETA_MULT_PROF 13		// Multiplicador por cada profundidad adicional en beta
		#define PODA_HASH_BETA_HAY_JH 0			// Multiplicador cuando sí hay jugada hash

		#define PODA_HASH_ALFA_BASE 99			// Diferencia base contra alfa
		#define PODA_HASH_ALFA_MULT_DIF 111		// Multiplicador por cada diferencia de profundidad adicional en alfa
		#define PODA_HASH_ALFA_MULT_PROF -10	// Multiplicador por cada profundidad adicional en alfa
		#define PODA_HASH_ALFA_HAY_JH -17		// Multiplicador cuando no hay jugada hash

		//
		// Poda hash beta
		// Último ajuste: 3.05
		//
		if (s32ProfHash >= s32Prof - PODA_HASH_PROF_DIF
			&& (iBoundHash == BOUND_LOWER || iBoundHash == BOUND_EXACTO)
			&& abs(s32Beta) < VICTORIA
			&& abs(s32EvalHash) < VICTORIA
			&& s32EvalHash > s32Beta + PODA_HASH_BETA_BASE + PODA_HASH_BETA_MULT_DIF * max(s32Prof - s32ProfHash, 1) + PODA_HASH_BETA_MULT_PROF * s32Prof + PODA_HASH_BETA_HAY_JH * (jugHash.u32Mov != 0))
		{
			return(s32Beta); // OJO: puedo devolver más, para hacerlo más fail soft (entre beta y eval hash)
		}

		//
		// Poda hash alfa
		// Último ajuste: 3.04
		//
		if (s32ProfHash >= s32Prof - PODA_HASH_PROF_DIF
			&& (iBoundHash == BOUND_EXACTO || iBoundHash == BOUND_UPPER)
			&& abs(s32Alfa) < VICTORIA
			&& abs(s32EvalHash) < VICTORIA
			&& s32EvalHash < s32Alfa - PODA_HASH_ALFA_BASE - PODA_HASH_ALFA_MULT_DIF * max(s32Prof - s32ProfHash, 1) - PODA_HASH_ALFA_MULT_PROF * s32Prof - PODA_HASH_ALFA_HAY_JH * (jugHash.u32Mov == 0))
		{
			return(s32Alfa); // OJO: puedo devolver menos, para hacerlo más fail soft (entre eval hash y alfa)
		}
	}
	const SINT32 s32ProfOriginal = s32Prof;

	//
	// EGTB (chikki)
	//
	if (pPos->u32MovExclu == 0 && pPos->u8NumPeonesB + pPos->u8NumPeonesN + pPos->u8NumPiezasB + pPos->u8NumPiezasN + 2 <= TB_LARGEST && pPos->u8Cincuenta == 0)
	{
		BOOL bTerminar = FALSE;
		unsigned tbres = tb_probe_wdl(pPos->u64TodasB,
			pPos->u64TodasN,
			BB_Mask(pPos->u8PosReyB) | BB_Mask(pPos->u8PosReyN),
			pPos->u64DamasB | pPos->u64DamasN,
			pPos->u64TorresB | pPos->u64TorresN,
			pPos->u64AlfilesB | pPos->u64AlfilesN,
			pPos->u64CaballosB | pPos->u64CaballosN,
			pPos->u64PeonesB | pPos->u64PeonesN,
			pPos->u8Cincuenta, pPos->u8Enroques,
			pPos->u8AlPaso == TAB_ALPASOIMPOSIBLE ? 0 : pPos->u8AlPaso, pPos->u8Turno);

		switch (tbres)
		{
			case 0:
				s32Alfa = TB_PIERDE + s32Ply;
				bTerminar = TRUE;
				break;
			case 1:
				s32Alfa = TABLAS - 3;
				bTerminar = TRUE;
				break;
			case 2:
				s32Alfa = TABLAS;
				bTerminar = TRUE;
				break;
			case 3:
				s32Alfa = TABLAS + 3;
				bTerminar = TRUE;
				break;
			case 4:
				s32Alfa = TB_GANA - s32Ply;
				bTerminar = TRUE;
				break;
			case 0xFFFFFFFF:
				break; // Creo que esto es cuando falla la consulta
			default:
				assert(0);
		}
		if (bTerminar)
		{
			if (tbres == 1 || tbres == 2 || tbres == 3)
			{
				if ((Pos_GetTurno(pPos) == BLANCAS && pPos->s32EvalMaterial > 0) || (Pos_GetTurno(pPos) == NEGRAS && pPos->s32EvalMaterial < 0))
					s32Alfa++;
				else if ((Pos_GetTurno(pPos) == BLANCAS && pPos->s32EvalMaterial < 0) || (Pos_GetTurno(pPos) == NEGRAS && pPos->s32EvalMaterial > 0))
					s32Alfa--;
			}
			GrabarHash(pPos, MAX_PLIES, EvalToTT(s32Alfa, s32Ply), JUGADA_NULA, -INFINITO, +INFINITO);
			return(s32Alfa);
		}
	}

	//
	// Ver si me puedo ahorrar una llamada a eval
	//
	if (Pos_GetJaqueado(pPos))
	{
		Pos_SetEval(pPos, NO_EVAL);
		bMejorando = FALSE;
	}
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

		bMejorando = Mejorando(pPos, s32Ply);
	}

	//
	// IIR
	// 
	#if defined(BUS_IIR_CUT)
		if (bEsCutNode && s32Prof >= 4 && Jug_GetEsNula(jugHash) && !pPos->u32MovExclu)
			s32Prof -= BUS_IIR_CUT;
	#elif defined(BUS_IIR_CUT_JC)
		if (bEsCutNode
			&& !pPos->u32MovExclu
			&& Jug_GetEsNula(jugHash)
			&& s32Prof >= 3
			&& pPos->s32Eval >= s32Beta
			&& abs(s32Alfa) < VICTORIA
			&& abs(s32Beta) < VICTORIA
			&& abs(pPos->s32Eval) < VICTORIA)
		{
			s32Prof -= BUS_IIR_CUT_JC;
		}
	#endif
	#if defined(BUS_IIR_ALL)
		if (!bEsCutNode && s32Prof >= 4 && Jug_GetEsNula(jugHash) && !pPos->u32MovExclu)
			s32Prof -= BUS_IIR_ALL;
	#elif defined(BUS_IIR_ALL_JC)
		if (bEsCutNode
			&& !pPos->u32MovExclu
			&& Jug_GetEsNula(jugHash)
			&& s32Prof >= 4
			&& pPos->s32Eval <= s32Alfa
			&& abs(s32Alfa) < VICTORIA
			&& abs(s32Beta) < VICTORIA
			&& abs(pPos->s32Eval) < VICTORIA)
		{
			s32Prof -= BUS_IIR_ALL_JC;
		}
	#endif

	//
	// Salida por profundidad agotada
	//
	if (s32Prof <= 0)
	{
		s32Val = QSConJaques(pPos, s32Alfa, s32Beta, s32Ply, QS_PROF_JAQUES);
		return(s32Val);
	}

	//
	// Razoring contra alfa
	//
	if (PodarRazoring(pPos, s32Alfa, s32Beta, s32Prof, s32Ply))
		return(s32Alfa);

	//
	// Podas contra beta: JC, null move, DD (sólo fuera de la PV)
	//
	if (PodarContraBeta(pPos, s32Ply, s32Prof, s32Beta, bMejorando))
	{
		aJugParaIID[s32Ply] = JUGADA_NULA;
		return(s32Beta);
	}
	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);

	//
	// IID
	//
	#if defined(BUS_IID)
		if (Jug_GetEsNula(jugHash)
			&& pPos->u32MovExclu == 0
			&& s32Prof >= 8
			&& pPos->s32Eval > s32Alfa)
		{
			//
			// No tengo jugada de la tabla hash, así que intento IID
			//
			s32Val = AlfaBetaPV(pPos, s32Alfa, s32Beta, s32Prof / 2, s32Ply + 2);
			if (dbDatosBusqueda.bAbortar)
				return(TABLAS);

			if (s32Val > s32Alfa)
				jugHash = aJugParaIID[s32Ply + 2];
		}
	#endif

	//
	// Borrar killers siguiente nivel
	//
	#if defined(BUS_BORRAR_KILLERS)
		if (s32Ply < MAX_PLIES - 2)
		{
			ajugKillers[s32Ply + 1][0] = JUGADA_NULA;
			ajugKillers[s32Ply + 1][1] = JUGADA_NULA;
		}
	#endif

	//
	// 
	// Jugada de la tabla hash (o de IID)
	// 
	//
	if (Jug_GetMov(jugHash)
		&& Jug_GetMov(jugHash) != pPos->u32MovExclu)
	{
		// Lo primero es comprobar que la jugada es pseudo-legal
		if (JugadaCorrecta(pPos, jugHash))
		{
			// Debo usar pJug para mantener la coherencia de las listas de jugadas al mover
			pJug = pPos->pListaJug;
			*pJug = jugHash;
			assert(JugadaCorrecta(pPos, *pJug));

			if (Mover(pPos, pJug))
			{
				dbDatosBusqueda.u32NumJugHash++;
				u32Legales = 1;
				s32Extensiones = Extensiones(pPos + 1, jugHash, s32Prof, s32Ply);

				#if defined(EXT_SINGULAR)
					// Alexandria 8:	margen doble = 10	!pv
					// Stash 37:		margen doble = 14	!pv
					// Horsie 1.0:		margen doble = 24	!pv
					// Superultra 2.1:	margen doble = 25	!pv
					// Zangdar 309:		margen doble = 50	!pv
					// Caissa 1.22:		margen doble = 17	!pv
					// PeaceKeeper3.01:	margen doble = 16	!pv
					// Obsidian 16:		margen doble = 13	!pv
					if (s32Extensiones == 0
						&& bEsCutNode	// Probar sin esta condición
						&& abs(s32EvalHash) < VICTORIA
						&& s32Prof >= 6
						&& s32ProfHash >= s32Prof - 3
						&& iBoundHash == BOUND_LOWER
						&& pPos->u32MovExclu == 0)
					{
						SINT32 s32SingBeta = s32EvalHash - s32Prof;

						pPos->u32MovExclu = jugHash.u32Mov;
						SINT32 s32SingVal = AlfaBeta(pPos, s32SingBeta - 1, s32SingBeta, (s32Prof - 1) / 2, s32Ply + 2, TRUE);
						pPos->u32MovExclu = 0;

						if (dbDatosBusqueda.bAbortar)
							return(TABLAS);

						if (s32SingVal < s32SingBeta)
						{
							s32Extensiones = 1;
							if (s32SingVal < s32SingBeta - EXT_SINGULAR
								&& s32Ply + s32Prof < dbDatosBusqueda.s32ProfRoot * 2)
							{
								s32Extensiones = 2;
							}
						}
						else if (s32SingVal >= s32Beta && abs(s32SingVal) < VICTORIA)
							// Multicut
							return(s32SingVal);
						else if (s32EvalHash >= s32Beta || bEsCutNode)
							// Extensión negativa
							s32Extensiones = -1;
						// Idea (hacer como obsidian 16, pero menos agresivo):
						/*	else if (ttScore >= beta) // Negative extensions
								extension = -3 + IsPV;
							else if (cutNode)
								extension = -2;
						*/ // Yo haría -2 y -1
						// Si hemos llegado hasta aquí, la búsqueda con exclusión ha cambiado el contenido de pPos+1, luego tenemos que repetir el Mover()
						pJug = pPos->pListaJug;
						*pJug = jugHash;
						Mover(pPos, pJug);
					}
				#endif // EXT_SINGULAR

				// Aquí irían las reducciones, pero no reducimos en la jugada de la tabla hash

				s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, FALSE, !bEsCutNode);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				if (s32Val > s32Mejor)
				{
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), jugHash, s32AlfaOriginal, s32Beta);
							aJugParaIID[s32Ply] = *pJug;
							#if defined(REFUTACION)
								Bus_Refuta_Grabar(pPos, *pJug);
							#endif
							return(s32Val);
						}
						s32Alfa = s32Val;
						jugMejor = jugHash;
					}
					bGrabarHash = TRUE;
					s32Mejor = s32Val;
				}
			} // Mover
		}
	} // if (jugHash)

	//
	// 
	// Jugada refutación
	// 
	//
#if defined(REFUTACION)
	TJugada jugRefuta = Bus_Refuta_Get(pPos);
	if (!Jug_GetEsNula(jugRefuta)
		&& Jug_GetMov(jugRefuta) != pPos->u32MovExclu
		&& Jug_GetMov(jugRefuta) != Jug_GetMov(jugHash))
	{
		// Lo primero es comprobar que la jugada es pseudo-legal
		if (JugadaCorrecta(pPos, jugRefuta))
		{
			// Debo usar pJug para mantener la coherencia de las listas de jugadas al mover
			pJug = pPos->pListaJug;
			*pJug = jugRefuta;
			assert(JugadaCorrecta(pPos, *pJug));

			if (Mover(pPos, pJug))
			{
				dbDatosBusqueda.u32NumJugHash++;
				u32Legales = 1;
				s32Extensiones = Extensiones(pPos + 1, jugRefuta, s32Prof, s32Ply);

				// Aquí irían las reducciones, pero estamos en la jugada de refutación, así que no reducimos

				s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, TRUE, TRUE);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				if (s32Val > s32Mejor)
				{
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), jugRefuta, s32AlfaOriginal, s32Beta);
							aJugParaIID[s32Ply] = *pJug;
							//Bus_Refuta_Grabar(pPos, *pJug); // No haría falta, grabaríamos la misma jugada que ya está
							return(s32Val);
						}
						s32Alfa = s32Val;
						jugMejor = jugRefuta;
					}
					bGrabarHash = TRUE;
					s32Mejor = s32Val;
				}
			} // if (Mover(pPos, pJug))
		} // if (JugadaCorrecta(pPos, jugRefuta))
	} // Jugada de refutación
#endif

	//
	//
	// Bucle principal
	// Primero: Capturas ganadoras
	//
	//
	pJug = GenerarCapturas(pPos, pPos->pListaJug, TRUE);
	AsignarValorSEE(pPos, pPos->pListaJug, pJug);

	// Mate killer: Si uno de los killers ha producido una eval de mate, se busca antes que las capturas ganadoras.
	//  Lo genero a continuación de pJug, y le doy un valor de ordenación más alto
    // OJO: ¿dónde estoy asignando el valor de ordenación? ¿O es el s32Val que lleva?
	if (Bus_DoyMatePronto(ajugKillers[s32Ply][0].s32Val))
	{
		if (JugadaCorrecta(pPos, ajugKillers[s32Ply][0]))
			*(++pJug) = ajugKillers[s32Ply][0];
	}
	if (Bus_DoyMatePronto(ajugKillers[s32Ply][1].s32Val))
	{
		if (JugadaCorrecta(pPos, ajugKillers[s32Ply][1]))
			*(++pJug) = ajugKillers[s32Ply][1];
	}

	if (pJug >= pPos->pListaJug)
	{
		// Ordenar capturas según SEE: Voy a parar (para generar el resto de los movimientos) en cuanto me encuentre
		//  una captura perdedora, así que necesito la lista totalmente ordenada, y luego no hago "cambiar"
		// OJO: es una tontería, porque con "cambiar" funcionaría igual, pero bueno...
		Ordenar(pPos, pJug);
		for (; pJug >= pPos->pListaJug; pJug--)
		{
			// Las capturas perdedoras las dejamos para el final
			if (Jug_GetVal(*pJug) < 0)
				break;

			// Jugada excluida
			if (pJug->u32Mov == pPos->u32MovExclu)
				continue;

		#if defined(REFUTACION)
			// Jugada refutación
			if (pJug->u32Mov == jugRefuta.u32Mov)
				continue;
		#endif

			// Mover y analizar
			assert(JugadaCorrecta(pPos, *pJug));
			if (Jug_GetMov(*pJug) == Jug_GetMov(jugHash) || !Mover(pPos, pJug))
				continue;
			u32Legales++;

			s32Extensiones = Extensiones(pPos + 1, *pJug, s32Prof, s32Ply);
			// Estamos en la fase de capturas ganadoras, así que no debemos reducir, ni siquiera LMR

			s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, FALSE, !bEsCutNode);
			if (dbDatosBusqueda.bAbortar)
				return(TABLAS);

			if (s32Val > s32Mejor)
			{
				if (s32Val > s32Alfa)
				{
					if (s32Val >= s32Beta)
					{
						GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), *pJug, s32AlfaOriginal, s32Beta);
						aJugParaIID[s32Ply] = *pJug;
						#if defined(REFUTACION)
							Bus_Refuta_Grabar(pPos, *pJug);
						#endif
						return(s32Val);
					}
					s32Alfa = s32Val;
					jugMejor = *pJug;
				}
				bGrabarHash = TRUE;
				s32Mejor = s32Val;
			}
		} // for (;pJug >= pPos->pListaJug;pJug--)
	} // if (pJug >= pPos->pListaJug)

	//
	//
	// Resto de jugadas + capturas perdedoras
	//
	//
	pJug = GenerarMovimientos(pPos, pJug+1);
	if (pJug >= pPos->pListaJug)
	{
		// En caso de FH, necesitaremos este puntero para recorrer las jugadas que no FH y ponerles una penalización de historia
		const TJugada * pJugHistoria = pJug;

		// Localizar killers y otras jugadas prometedoras, como sacar piezas de casillas atacadas por el rival
		LocalizarJugadasPrometedoras(pPos, pPos->pListaJug, pJug, s32Ply);

		for (; pJug >= pPos->pListaJug; pJug--)
		{
			CambiarJugada(pPos,pJug);
			assert(JugadaCorrecta(pPos, *pJug));

			// Jugada excluida y jugada hash
			if (pJug->u32Mov == pPos->u32MovExclu || (Jug_GetMov(*pJug) == Jug_GetMov(jugHash)))
				continue;

			#if defined(REFUTACION)
				if (pJug->u32Mov == jugRefuta.u32Mov)
					continue;
			#endif

			// Poda SEE negativo (supongo que también poda las jugadas no capturas con historia <= -100)
			#if defined(PODA_SEE_NEGATIVO)
				#if PODA_SEE_NEGATIVO == 1
					if (s32Prof <= 3 && pJug->s32Val <= -VAL_PEON)
					{
						s32Val = s32Alfa;
						if (s32Val > s32Mejor)
						{
							s32Mejor = s32Val;
							bGrabarHash = FALSE;
						}
						continue;
					}
				#elif PODA_SEE_NEGATIVO == 2
					if (pJug->s32Val <= -VAL_PEON - s32Prof * VAL_PEON)
					{
						s32Val = s32Alfa;
						if (s32Val > s32Mejor)
						{
							s32Mejor = s32Val;
							bGrabarHash = FALSE;
						}
						continue;
					}
				#endif
			#endif

			// Mover y analizar
			if (!Mover(pPos, pJug))
				continue;
			u32Legales++;

			// Intentar poda futility
			#if defined(PODA_FUTIL)
				if (PodarFutility(pPos, s32Prof, s32Alfa))
				{
					s32Val = s32Alfa;
					if (s32Val > s32Mejor)
					{
						s32Mejor = s32Val;
						bGrabarHash = FALSE;
					}
					continue;
				}
			#endif

			// Late move pruning
			#if defined(PODA_LMP)
				if (PodarLMP(s32Prof, u32Legales + 1, pPos))
				{
					s32Val = s32Alfa;
					if (s32Val > s32Mejor)
					{
						s32Mejor = s32Val;
						bGrabarHash = FALSE;
					}
					continue;
				}
			#endif

			// No hay poda, analizo
			s32Extensiones = Extensiones(pPos + 1, *pJug, s32Prof, s32Ply);
			if (!s32Extensiones)
				s32Extensiones = Reducciones(pPos + 1, *pJug, s32Prof, s32Alfa, u32Legales, bEsCutNode, bMejorando);

			s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, FALSE, !bEsCutNode);
			if (dbDatosBusqueda.bAbortar)
				return(TABLAS);

			// Rebúsqueda tras reducción
			// Si hemos reducido la profundidad y no hemos FL, algo pasa. Se supone que reducimos en la tranquilidad de
			//  que ésta es una jugada mala que no tiene ninguna posibilidad de proporcionar algo interesante. Por lo
			//  tanto, lo que vamos a hacer es aumentar de nuevo la profundidad y comprobar si, en efecto, obtenemos un
			//  resultado positivo con una profundidad adecuada (reducción cero)
			if (s32Extensiones < 0 && s32Val > s32Mejor && s32Val > s32Alfa)
			{
				s32Extensiones = 0;
				s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, FALSE, !bEsCutNode);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);
			}

			// Ya tengo un valor, veamos cómo es de bueno
			if (s32Val > s32Mejor)
			{
				if (s32Val > s32Alfa)
				{
					if (s32Val >= s32Beta)
					{
						GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), *pJug, s32AlfaOriginal, s32Beta);
						Jug_SetVal(pJug, s32Val);
						ActualizarKillers(*pJug, s32Ply);
						aJugParaIID[s32Ply] = *pJug;

						// Historia
						if (pPos->s32EvalMaterial == (pPos + 1)->s32EvalMaterial)	// No cambia material = jugada tranquila
						{
							Bus_Historia_Sumar(*pJug, s32ProfOriginal);
							for (TJugada* pJ = pJugHistoria; pJ > pJug; pJ--)
								Bus_Historia_Restar(*pJ, s32ProfOriginal);
						}

						#if defined(REFUTACION)
							Bus_Refuta_Grabar(pPos, *pJug);
						#endif
						return(s32Val);
					}
					s32Alfa = s32Val;
					jugMejor = *pJug;
				}
				s32Mejor = s32Val;
				bGrabarHash = TRUE;
			}
		} // for (;pJug >= pPos->pListaJug;pJug--)
	} // if (pJug >= pPos->pListaJug)

	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);

	if (pPos->u32MovExclu != 0)
		bGrabarHash = FALSE;

	//
	// Devolver fail low o exacta
	//
	if (u32Legales)
	{
		if (bGrabarHash)
			GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Mejor, s32Ply), jugMejor, s32AlfaOriginal, s32Beta);
		aJugParaIID[s32Ply] = jugMejor;
	}
	else
	{
		if (pPos->u32MovExclu != 0)
			return(-INFINITO);
		if (Pos_GetJaqueado(pPos))
			s32Mejor = -INFINITO + s32Ply;
		else
			s32Mejor = TABLAS;
		if (bGrabarHash)
			GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Mejor, s32Ply), JUGADA_NULA, -INFINITO, +INFINITO);
		aJugParaIID[s32Ply] = JUGADA_NULA;
	}
	return(s32Mejor);
}

/*
 *
 * AlfaBetaPV
 *
 */
SINT32 AlfaBetaPV(TPosicion * pPos, SINT32 s32Alfa, SINT32 s32Beta, SINT32 s32Prof, SINT32 s32Ply)
{
	SINT32 s32Extensiones = 0;
	SINT32 s32Val;
	SINT32 s32Mejor = -INFINITO + s32Ply;	// Para fail-soft
	const SINT32 s32AlfaOriginal = s32Alfa;
	UINT32 u32Legales = 0;
	TJugada * pJug;					// Para la generación de jugadas
	TJugada	jugMejor = JUGADA_NULA;
	BOOL bMejorando;

	//
	// Verificaciones de consistencia
	//
	assert(s32Alfa >= -INFINITO);
	assert(s32Alfa < s32Beta);
	assert(s32Beta <= INFINITO);
	if (s32Prof < 0)
      s32Prof = 0;
	assert(s32Ply > 0);

	//
	// Comprobaciones de salida anticipada
	//
	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);
	if (pPos->u8Cincuenta > 100)
		return(TABLAS);
	if (SegundaRepeticion(pPos))
		return(TABLAS);
	if (InsuficienteMaterial(pPos))
		return(TABLAS);
	if (s32Ply >= MAX_PLIES - 1)
		return(TABLAS);

	//
	// Leemos el reloj cada 4096 nodos - también pongo aquí el parseo de comandos
	//
	if ((dbDatosBusqueda.u32NumNodos & 0xFFF) == 0xFFF)
	{
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
	}

	//
	// Salida por profundidad agotada
	//
	if (s32Prof <= 0)
	{
		s32Val = QSConJaques(pPos, s32Alfa, s32Beta, s32Ply, QS_PROF_JAQUES);
		return(s32Val);
	}

	//
	// Poda de distancia a mate
	//
	s32Alfa = max(-INFINITO + s32Ply, s32Alfa);
	s32Beta = min(INFINITO - s32Ply - 1, s32Beta);
	if (s32Alfa >= s32Beta)
		return(s32Alfa);

	//
	// Consultar tabla hash
	//
	TNodoHash * pNodoHash = NULL;
	TJugada jugHash = JUGADA_NULA;
	SINT32 s32EvalHash = -INFINITO;
	SINT32 s32ProfHash = 0;
	int iBoundHash = 0;
	if (pPos->u32MovExclu == 0 && ConsultarHash(pPos, s32Prof, s32Alfa, s32Beta, &pNodoHash))
	{
		if (!Jug_GetEsNula(pNodoHash->jug))
			jugHash = aJugParaIID[s32Ply] = pNodoHash->jug;

		if (2 * s32Ply > s32Prof && pPos->u8Cincuenta < 90 && !Bus_DoyMatePronto(abs(pNodoHash->jug.s32Val)))
			return(EvalFromTT(pNodoHash->jug.s32Val, s32Ply));
	}
	if (pNodoHash != NULL)
	{
		jugHash = pNodoHash->jug;
		s32EvalHash = EvalFromTT(pNodoHash->jug.s32Val, s32Ply);
		s32ProfHash = pNodoHash->u8Prof;
		iBoundHash = Hash_GetBound(pNodoHash);
		if (!Jug_GetEsNula(jugHash))
			aJugParaIID[s32Ply] = jugHash;
	}
	const SINT32 s32ProfOriginal = s32Prof;

	//
	// EGTB (chikki)
	//
	UINT8 u8Cincuenta = pPos->u8Cincuenta;
	if (pPos->u32MovExclu == 0 && pPos->u8NumPeonesB + pPos->u8NumPeonesN + pPos->u8NumPiezasB + pPos->u8NumPiezasN + 2 <= TB_LARGEST && u8Cincuenta == 0)
	{
		BOOL bTerminar = FALSE;	// 21/02/25 - Ordeno un poquito el código subsiguiente para permitir grabar hash
		unsigned tbres = tb_probe_wdl(pPos->u64TodasB,
			pPos->u64TodasN,
			BB_Mask(pPos->u8PosReyB) | BB_Mask(pPos->u8PosReyN),
			pPos->u64DamasB | pPos->u64DamasN,
			pPos->u64TorresB | pPos->u64TorresN,
			pPos->u64AlfilesB | pPos->u64AlfilesN,
			pPos->u64CaballosB | pPos->u64CaballosN,
			pPos->u64PeonesB | pPos->u64PeonesN,
			u8Cincuenta, pPos->u8Enroques,
			pPos->u8AlPaso == TAB_ALPASOIMPOSIBLE ? 0 : pPos->u8AlPaso, pPos->u8Turno);

		switch (tbres)
		{
			case 0:
				s32Alfa = TB_PIERDE + s32Ply;
				bTerminar = TRUE;
				break;
			case 1:
				s32Alfa = TABLAS - 3;
				bTerminar = TRUE;
				break;
			case 2:
				s32Alfa = TABLAS;
				bTerminar = TRUE;
				break;
			case 3:
				s32Alfa = TABLAS + 3;
				bTerminar = TRUE;
				break;
			case 4:
				s32Alfa = TB_GANA - s32Ply;
				bTerminar = TRUE;
				break;
			case 0xFFFFFFFF:
				break; // Creo que esto es cuando falla la consulta
			default:
				assert(0);
		}
		if (bTerminar)
		{
			if (tbres == 1 || tbres == 2 || tbres == 3)
			{
				if ((Pos_GetTurno(pPos) == BLANCAS && pPos->s32EvalMaterial > 0) || (Pos_GetTurno(pPos) == NEGRAS && pPos->s32EvalMaterial < 0))
					s32Alfa++;
				else if ((Pos_GetTurno(pPos) == BLANCAS && pPos->s32EvalMaterial < 0) || (Pos_GetTurno(pPos) == NEGRAS && pPos->s32EvalMaterial > 0))
					s32Alfa--;
			}
			GrabarHash(pPos, MAX_PLIES, EvalToTT(s32Alfa, s32Ply), JUGADA_NULA, -INFINITO, +INFINITO);
			return(s32Alfa);
		}
	}

	//
	// Mejorar eval si es posible
	//
	if (Pos_GetJaqueado(pPos))
	{
		Pos_SetEval(pPos, NO_EVAL);
		bMejorando = FALSE;
	}
	else
	{
		pPos->s32Eval = Evaluar(pPos);
		if ((iBoundHash == BOUND_EXACTO && pPos->s32Eval != s32EvalHash)
			|| (iBoundHash == BOUND_UPPER && s32EvalHash < pPos->s32Eval)
			|| (iBoundHash == BOUND_LOWER && s32EvalHash > pPos->s32Eval))
		{
			if (abs(s32EvalHash) < VICTORIA && pPos->s32Eval != s32EvalHash)
			{
				pPos->s32Eval = s32EvalHash;
				GrabarHashEvalSoloEval(pPos, s32EvalHash);
			}
		}
		bMejorando = Mejorando(pPos, s32Ply);
	}

	//
	// IIR
	// 
	#if defined(BUS_IIR_PV)
		if (s32Prof >= 4 && Jug_GetEsNula(jugHash) && !pPos->u32MovExclu) // Idea: no jughash o jughash con prof muy baja
			s32Prof -= BUS_IIR_PV;
	#endif

	//
	// Salida por profundidad agotada
	//
	if (s32Prof <= 0)
	{
		s32Val = QSConJaques(pPos, s32Alfa, s32Beta, s32Ply, QS_PROF_JAQUES);
		return(s32Val);
	}

	//
	// IID
	//
	#if defined(BUS_IID)
		if (Jug_GetEsNula(jugHash)
			&& pPos->u32MovExclu == 0 
			&& s32Prof >= 5)
		{
			//
			// No tengo jugada de la tabla hash, así que intento IID
			// 
			s32Val = AlfaBetaPV(pPos, s32Alfa, s32Beta, s32Prof - 2, s32Ply + 2);
			if (dbDatosBusqueda.bAbortar)
				return(TABLAS);

			if (s32Val > s32Alfa)
				jugHash = aJugParaIID[s32Ply + 2];
		}
	#endif

	//
	// Borrar killers siguiente nivel
	//
	#if defined(BUS_BORRAR_KILLERS)
		if (s32Ply < MAX_PLIES - 2)
		{
			ajugKillers[s32Ply + 1][0] = JUGADA_NULA;
			ajugKillers[s32Ply + 1][1] = JUGADA_NULA;
		}
	#endif

	//
	// 
	// Jugada de la tabla hash
	// 
	//
	if (!Jug_GetEsNula(jugHash)
		&& Jug_GetMov(jugHash) != pPos->u32MovExclu)
	{
		// Lo primero es comprobar que la jugada es pseudo-legal
		if (JugadaCorrecta(pPos, jugHash))
		{
			// Debo usar pJug para mantener la coherencia de las listas de jugadas al mover
			pJug = pPos->pListaJug;
			*pJug = jugHash;
			assert(JugadaCorrecta(pPos, *pJug));

			if (Mover(pPos, pJug))
			{
				dbDatosBusqueda.u32NumJugHash++;
				u32Legales = 1;
				s32Extensiones = Extensiones(pPos + 1, jugHash, s32Prof, s32Ply);

				#if defined(EXT_SINGULAR)
					// SF 17.1:			prof >= 6; singbeta = ttData.value - (59 + 77 * (ss->ttPv && !PvNode)) * depth / 54;
					// Clover 8.1:		prof >= 5; singbeta = ttValue - (SEMargin + 64 * (!pvNode && was_pv)) * depth / 64;
					// Senpai:			prof >= 6; singalfa = evalhash - 50
					// PeaceKeeper3.01:	prof >= 6; singbeta = evalhash - prof * 63/16	(3.9375)
					// Astra 5.1:		prof >= 6; singbeta = evalhash - prof * 3
					// Arcanum 2.4:		prof >= 7; singbeta = evalhash - prof * 3
					// Superultra 2.1:	prof >= 7; singbeta = evalhash - prof * 3
					// Zangdar 309:		prof >  8; singbeta = evalhash - prof * 2
					// Kobra 2.0		prof >= 8; singbeta = evalhash - prof * 2
					// Demolito 2025:	prof >= 5; singbeta = evalhash - prof * 2
					// Arasan 25:		prof >= 8; singbeta = evalhash - prof * 100/64	(1.5625)
					// Alexandria 8:	prof >= 6; singbeta = evalhash - prof * 5/8		(0.625)
					// Berserk:			prof >= 6; singbeta = evalhash - prof * 5/8		(0.625)
					// Horsie 1.0:		prof >= 5; singbeta = evalhash - prof * 11/10
					// RukChess:		prof >= 8; singbeta = evalhash - prof
					// Altair 7.1.5:	prof >= 6; singbeta = evalhash - prof
					// Patricia 4:		prof >= 5; singbeta = evalhash - prof
					// Caissa 1.22:		prof >= 4; singbeta = evalhash - prof
					// Obsidian 16:		prof >= 5; singbeta = evalhash - prof
					if (s32Extensiones == 0
						&& abs(s32EvalHash) < VICTORIA
						&& s32Prof >= 6
						&& s32ProfHash >= s32Prof - 3
						&& iBoundHash == BOUND_LOWER
						&& pPos->u32MovExclu == 0)
					{
						SINT32 s32SingBeta = s32EvalHash - s32Prof;

						pPos->u32MovExclu = jugHash.u32Mov;
						SINT32 s32SingVal = AlfaBeta(pPos, s32SingBeta - 1, s32SingBeta, (s32Prof - 1) / 2, s32Ply + 2, TRUE);
						pPos->u32MovExclu = 0;

						if (dbDatosBusqueda.bAbortar)
							return(TABLAS);

						if (s32SingVal < s32SingBeta)
							s32Extensiones = 1;
						else if (s32SingVal >= s32Beta && abs(s32SingVal) < VICTORIA)
							// Multicut
							return(s32SingVal);
						else if (s32EvalHash >= s32Beta || s32EvalHash <= s32Alfa)
							// Extensión negativa en nodo PV
							s32Extensiones = -1;
						// Si hemos llegado hasta aquí, la búsqueda con exclusión ha cambiado el contenido de pPos+1, luego tenemos que repetir el Mover()
						pJug = pPos->pListaJug;
						*pJug = jugHash;
						Mover(pPos, pJug);
					}
				#endif // EXT_SINGULAR

				// Aquí irían las reducciones, pero estamos en la jugada de la tabla hash, así que no reducimos

				s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, TRUE, FALSE);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				if (s32Val > s32Mejor)
				{
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), jugHash, s32AlfaOriginal, s32Beta);
							aJugParaIID[s32Ply] = *pJug;
							#if defined(REFUTACION)
								Bus_Refuta_Grabar(pPos, *pJug);
							#endif
							return(s32Val);
						}
						s32Alfa = s32Val;
						jugMejor = jugHash;
					}
					s32Mejor = s32Val;
				}
			} // if (Mover(pPos,pJug))
		} // if (JugadaCorrecta(pPos,jugHash))
	} // Jugada de la tabla hash

	//
	// 
	// Jugada refutación
	// 
	//
#if defined(REFUTACION)
	TJugada jugRefuta = Bus_Refuta_Get(pPos);
	if (!Jug_GetEsNula(jugRefuta)
		&& Jug_GetMov(jugRefuta) != pPos->u32MovExclu
		&& Jug_GetMov(jugRefuta) != Jug_GetMov(jugHash))
	{
		// Lo primero es comprobar que la jugada es pseudo-legal
		if (JugadaCorrecta(pPos, jugRefuta))
		{
			// Debo usar pJug para mantener la coherencia de las listas de jugadas al mover
			pJug = pPos->pListaJug;
			*pJug = jugRefuta;
			assert(JugadaCorrecta(pPos, *pJug));

			if (Mover(pPos, pJug))
			{
				u32Legales = 1;
				s32Extensiones = Extensiones(pPos + 1, jugRefuta, s32Prof, s32Ply);

				// Aquí irían las reducciones, pero estamos en la jugada de refutación, así que no reducimos

				s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, TRUE, TRUE);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);

				if (s32Val > s32Mejor)
				{
					if (s32Val > s32Alfa)
					{
						if (s32Val >= s32Beta)
						{
							GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), jugRefuta, s32AlfaOriginal, s32Beta);
							aJugParaIID[s32Ply] = *pJug;
							//Bus_Refuta_Grabar(pPos, *pJug); // No haría falta, grabaríamos la misma jugada que ya está
							return(s32Val);
						}
						s32Alfa = s32Val;
						jugMejor = jugRefuta;
					}
					s32Mejor = s32Val;
				}
			} // if (Mover(pPos, pJug))
		} // if (JugadaCorrecta(pPos, jugRefuta))
	} // Jugada de refutación
#endif

	//
	// 
	// Bucle principal
	// Primero: Capturas ganadoras
	// 
	//
	pJug = GenerarCapturas(pPos, pPos->pListaJug, TRUE);
	AsignarValorSEE(pPos, pPos->pListaJug, pJug);

	// Mate killer:
	// Si uno de los killers ha producido una eval de mate, se busca antes que las capturas ganadoras
	// Lo genero a continuación de pJug, y le doy un valor de ordenación más alto
	if (Bus_DoyMatePronto(ajugKillers[s32Ply][0].s32Val))
	{
		if (JugadaCorrecta(pPos, ajugKillers[s32Ply][0]))
			*(++pJug) = ajugKillers[s32Ply][0];
	}
	if (Bus_DoyMatePronto(ajugKillers[s32Ply][1].s32Val))
	{
		if (JugadaCorrecta(pPos, ajugKillers[s32Ply][1]))
			*(++pJug) = ajugKillers[s32Ply][1];
	}

	// Ordenamos las jugadas y empezamos a buscar
	if (pJug >= pPos->pListaJug)
	{
		// Ordenar capturas según SEE: Voy a parar (para generar el resto de los movimientos) en cuanto me encuentre
		//  una captura perdedora, así que necesito la lista totalmente ordenada, y luego no hago "cambiar"
		Ordenar(pPos,pJug);
		for (; pJug >= pPos->pListaJug; pJug--)
		{
			// Las capturas perdedoras las dejamos para el final
			if (Jug_GetVal(*pJug) < 0)
				break;

			// Jugada excluida
			if (pJug->u32Mov == pPos->u32MovExclu)
				continue;

		#if defined(REFUTACION)
			// Jugada refutación
			if (pJug->u32Mov == jugRefuta.u32Mov)
				continue;
		#endif

			// Mover y analizar
			assert(JugadaCorrecta(pPos, *pJug));
			if (Jug_GetMov(*pJug) == Jug_GetMov(jugHash) || !Mover(pPos, pJug))
				continue;
			u32Legales++;

			s32Extensiones = Extensiones(pPos + 1, *pJug, s32Prof, s32Ply);
			// Estamos en la fase de capturas ganadoras, así que no debemos reducir, ni siquiera LMR

			s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, u32Legales == 1, u32Legales > 1);
			if (dbDatosBusqueda.bAbortar)
				return(TABLAS);

			if (s32Val > s32Mejor)
			{
				if (s32Val > s32Alfa)
				{
					if (s32Val >= s32Beta)
					{
						GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), *pJug, s32AlfaOriginal, s32Beta);
						aJugParaIID[s32Ply] = *pJug;
						#if defined(REFUTACION)
							Bus_Refuta_Grabar(pPos, *pJug);
						#endif
						return(s32Val);
					}
					s32Alfa = s32Val;
					jugMejor = *pJug;
				}
				s32Mejor = s32Val;
			}
		} // for (;pJug >= pPos->pListaJug;pJug--)
	} // if (pJug >= pPos->pListaJug)

	//
	// 
	// Resto de jugadas + capturas perdedoras
	// 
	//
	pJug = GenerarMovimientos(pPos, pJug+1);
	if (pJug >= pPos->pListaJug)
	{
		// En caso de FH, necesitaremos este puntero para recorrer las jugadas que no FH y ponerles una penalización de historia
		TJugada* pJugHistoria = pJug;

		// Localizar killers y otras jugadas prometedoras (como mover piezas atacadas a casillas no atacadas)
		LocalizarJugadasPrometedoras(pPos, pPos->pListaJug, pJug, s32Ply);
		for (; pJug >= pPos->pListaJug; pJug--)
		{
			CambiarJugada(pPos, pJug);

			assert(JugadaCorrecta(pPos, *pJug));

			// Jugada excluida y hash y refutación
			if (pJug->u32Mov == pPos->u32MovExclu || (Jug_GetMov(*pJug) == Jug_GetMov(jugHash)))
				continue;

			#if defined(REFUTACION)
				if (pJug->u32Mov == jugRefuta.u32Mov)
					continue;
			#endif
			
			// Poda SEE negativo (supongo que también poda las jugadas no capturas con historia <= -100)
			#if defined(PODA_SEE_NEGATIVO)
				#if PODA_SEE_NEGATIVO == 1
					if (s32Prof <= 3 && pJug->s32Val <= -VAL_PEON)
					{
						s32Val = s32Alfa;
						if (s32Val > s32Mejor)
							s32Mejor = s32Val;
						continue;
					}
				#elif PODA_SEE_NEGATIVO == 2
					if (pJug->s32Val <= -VAL_PEON - s32Prof * VAL_PEON)
					{
						s32Val = s32Alfa;
						if (s32Val > s32Mejor)
							s32Mejor = s32Val;
						continue;
					}
				#endif
			#endif

			// Mover y analizar
			if (!Mover(pPos, pJug))
				continue;
			u32Legales++;

			// Intentar poda futility
			#if defined(PODA_FUTIL)
				if (u32Legales > 1 && PodarFutility(pPos, s32Prof, s32Alfa))
				{
					s32Val = s32Alfa; // No me arriesgo a poner la eval. Alfa es más seguro
					continue;
				}
			#endif

			// Late move pruning
			#if defined(PODA_LMP)
				if (PodarLMP(s32Prof, u32Legales + 1, pPos))
				{
					s32Val = s32Alfa;
					if (s32Val > s32Mejor)
						s32Mejor = s32Val;
					continue;
				}
			#endif
				
			// Idea: poda de historia -> si no es captura y la historia del movimiento es muy mala (más mala cuanta más prof queda), podar

			// No hay poda, analizo
			s32Extensiones = Extensiones(pPos + 1, *pJug, s32Prof, s32Ply);
			if (!s32Extensiones)
				s32Extensiones = Reducciones(pPos + 1, *pJug, s32Prof, s32Alfa, u32Legales, FALSE, bMejorando);

			s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, u32Legales == 1, u32Legales > 1);
			if (dbDatosBusqueda.bAbortar)
				return(TABLAS);

			// Rebúsqueda tras reducción
			// Si hemos reducido la profundidad y no hemos FL, algo pasa. Se supone que reducimos en la tranquilidad de
			//  que ésta es una jugada mala que no tiene ninguna posibilidad de proporcionar algo interesante. Por lo
			//  tanto, lo que vamos a hacer es aumentar de nuevo la profundidad y comprobar si, en efecto, obtenemos un
			//  resultado positivo con una profundidad adecuada (cero)
			if (s32Extensiones < 0 && s32Val > s32Mejor && s32Val > s32Alfa)
			{
				s32Extensiones = 0;
				s32Val = Analizar(pPos, s32Alfa, s32Beta, s32Prof, s32Extensiones, s32Ply, FALSE, TRUE);
				if (dbDatosBusqueda.bAbortar)
					return(TABLAS);
			}

			//
			// Ya tengo un valor, veamos cómo es de bueno
			//
			if (s32Val > s32Mejor)
			{
				if (s32Val > s32Alfa)
				{
					if (s32Val >= s32Beta)
					{
						GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Val, s32Ply), *pJug, s32AlfaOriginal, s32Beta);
						Jug_SetVal(pJug, s32Val);
						ActualizarKillers(*pJug, s32Ply);
						aJugParaIID[s32Ply] = *pJug;

						// Historia
						if (pPos->s32EvalMaterial == (pPos + 1)->s32EvalMaterial)	// No cambia material = jugada tranquila
						{
							Bus_Historia_Sumar(*pJug, s32ProfOriginal);
							for (TJugada* pJ = pJugHistoria; pJ > pJug; pJ--)
								Bus_Historia_Restar(*pJ, s32ProfOriginal);
						}

						#if defined(REFUTACION)
							Bus_Refuta_Grabar(pPos, *pJug);
						#endif
						return(s32Val);
					}
					s32Alfa = s32Val;
					jugMejor = *pJug;
				}
				s32Mejor = s32Val;
			}
		} // for (;pJug >= pPos->pListaJug;pJug--)
	} // if (pJug >= pPos->pListaJug)

	if (dbDatosBusqueda.bAbortar)
		return(TABLAS);

	//
	// Devolver fail low o exacta
	//
	if (u32Legales)
	{
		if (pPos->u32MovExclu == 0)
		{
			if (s32Mejor > s32AlfaOriginal && s32Mejor < s32Beta)
				Bus_ActualizarPV(jugMejor, s32Ply);
			GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Mejor, s32Ply), jugMejor, s32AlfaOriginal, s32Beta);
			aJugParaIID[s32Ply] = jugMejor;
		}
		return(s32Mejor);
	}
	else
	{
		if (pPos->u32MovExclu != 0)
			return(-INFINITO);
		if (Pos_GetJaqueado(pPos))
			s32Mejor = -INFINITO + s32Ply;
		else
			s32Mejor = TABLAS;
		GrabarHash(pPos, s32ProfOriginal, EvalToTT(s32Mejor, s32Ply), JUGADA_NULA, -INFINITO, +INFINITO);
		aJugParaIID[s32Ply] = JUGADA_NULA;
		return(s32Mejor);
	}
}
