/*
	Anubis

	José Carlos Martínez Galán
*/

#pragma once
#if !defined(WIN64) && !defined(__linux__)
#define WIN64
#endif
#pragma warning(disable: 4996)
#pragma warning(disable: 4324)

/////////////////////////////////////////////////
//
#define VERS_304
//
//		Pte:
//			Poner PODA_JC como un número que sea la prof máxima a partir de la cual podemos podar (ahora mismo es 4 a piñón; podríamos probar 5, 6, ...) (Berserk usa 9)
// 
//			IIR en lugar de IID (no las dos cosas juntas) - explicación: igual que analizo prof 1 antes que prof 2 para ordenar el árbol, si me encuentro una rama desordenada (no hash move)
//				puedo hacer una búsqueda con prof reducida para ordenarlo, pero no rebusco otra vez a prof nominal de inmediato, sino que lo dejo ahí para la próxima vez que la búsqueda
//				pase por ese nodo
// 
//			Tabla hash: si tengo una Ubound x a prof d, y estoy buscando con prof < d, y ese x no es < alfa, no puedo podar, pero podría usar el dato para limitar el
//						resultado devuelto. Si mi búsqueda, a prof d-1, devuelve x+10, yo sé que en prof d es como máximo x, luego puedo devolver x, en vez de x+10
// 
//			Ideas: Ver 'Main search loop' de Laser 1.7
//				 : Ver PROBCUT en RukChess
//				 : Probar QS_PODA_FUTIL más pequeño (Hakka rel 3 utiliza 50)
//				 : Ajuste de tiempo en Astra 5.1, bestMove()
//				 : Extensión inesperada -> cuando es muy obvio que espero fail low, y ocurre fail high o al revés, extender y repetir la búsqueda en ese nodo
// 
//			Ordenación: tengo que mejorar la ordenación, con alguna heurística tipo history o countermove o incluso https://www.chessprogramming.org/Last_Best_Reply
//						ver RukChess COUNTER_MOVE_HISTORY
//						https://www.chessprogramming.org/History_Heuristic
//						Ver Obsidian 16 (buscar '// Update histories' en search.cpp)
// 
//			Razoring: probar modo menos arriesgado (tipo SF 17.1)
// 
//			Eval static en tabla hash normal, en vez de una especial (problema porque en la hash eval almaceno muchos bitboards)
//			IIR: jugar con la prof de corte (ahora mismo >= 4 a piñón)
//			En AlfaBeta, cuando estoy en excluded move, ya no necesito evaluar porque esa posición ya la he evaluado antes de esta llamada ¿no?
//			Idea: ver código de extensión singular en Alfabeta
// 
//			3.04_:
//				Descarto para siempre RED_FH (ver al final de este archivo los resultados del estudio estadístico)
//				Quito Bus_EvalRepe() porque estoy cansado de la oscilación que no sirve para nada
// 
//			3.04a:
//				BUS_IIR_CUT_JC (prof >= 5 y eval > Alfa)
// 
//			3.04b:
//				BUS_IIR_CUT_JC (prof >= 4 y eval >= Beta y no jugHash)
// 
//			3.04c:
//				BUS_IIR_CUT_JC (prof >= 3 y eval >= Beta y no jugHash)
// 
//			3.04d:
//				BUS_IIR_CUT 1 (antiguo, sin cambios)
//				BUS_IIR_ALL_JC (prof >= 4 y eval <= Alfa y no jugHash)
// 
//			3.04e:
//				BUS_IIR_CUT 1 (antiguo, sin cambios)
//				BUS_IIR_ALL 1 (antiguo, sin cambios)
//				(alfa 50) -> 115 0 23 -9
// 
//			3.04f:
//				(alfa 100) -> 90 0 49 9
// 
//			3.04g:
//				(alfa 500) -> 205 175 30 -9
// 
//			3.04h:
//				(alfa 1000) -> 500 365 33 5
// 
//			3.04i:
//				(alfa 5000) -> 495 365 34 9
// 
//			3.04j:
//				(deepseek) -> 420 25 45 2
// 
//			3.04k:
//				(deepseek) -> 380 35 50 -3
// 
//			3.04l:
//				(deepseek) -> 450 20 42 5
// 
//			3.04m:
//				(deepseek) -> 300 40 47 1
// 
//			3.04n:
//				(deepseek) -> 410 28 52 -2
// 
//			3.04o:
//				(deepseek) -> 370 32 44 4
// 
//			3.04p:
//				(claude) -> 485 350 32 7
// 
//			3.04q:
//				(claude) -> 510 365 35 1
// 
//			3.04r:
//				(claude) -> 475 380 31 12
// 
//			3.04s:
//				(claude) -> 520 340 36 5
// 
//			3.04t:
//				(claude) -> 460 365 33 8
// 
//			3.04u:
// 				(yo) -> 95 7 40 0
// 
//			3.04v:
// 				(yo) -> 1 1 1 1
// 
//			3.04w:
// 				(claude_patrones) -> 102 5 39 8
// 
//			3.04x:
// 				(claude_patrones2) -> 414 6 37 -5
// 
//			3.04y:
// 				(yo) -> 102 5 39 -10
// 
//			3.04z:
// 				(claude_gemi_patrones) -> -8 372 29 11
// 
//			3.04aa:
// 				(claude_gemi_patrones) -> -45 186 29 15
// 
//			3.04ab:
// 				(cl_gm_cop) -> 62 57 -2 -6
// 
//			3.04ac:
// 				(cl_gm_cop) -> 277 426 -5 -5
// 
//			3.04ad:
// 				(cl_gm_cop) -> 40 301 6 -7
// 
//			3.04ae:
// 				(claude_gemi_patrones) -> 24 332 45 -10
// 
//			3.04af:
// 				(cl_gm_cop) -> 568 103 51 0
// 
//			3.04ag:
// 				(cl_gm_cop) -> 356 406 33 6
// 
//			3.04ah:
// 				-53 -6 -10 16
// 
//			3.04ai:
// 				180 218 54 4
// 
//			3.04aj:
// 				122 266 -2 14
// 
//			3.04ak:
// 				14 320 24 -1
// 
//			3.04al:
// 				-31 462 18 -11
// 
//			3.04am:
// 				497 369 -10 -12
// 
//			3.04an:
// 				548 113 59 -17
// 
//			3.04ao:
// 				148 304 13 13
// 
//			3.04ap:
// 				85 289 -5 11
// 
//			3.04aq:
// 				403 343 -8 -5
// 
//			3.04ar:
// 				494 304 8 11
// 
//			3.04as:
// 				366 314 -7 -4
// 
//			3.04at:
// 				469 320 -15 -2
// 
//			3.04au:
// 				621 367 -20 19
// 
//			3.04av:
// 				457 275 9 12
// 
//			3.04aw:
// 				494 304 -8 11
// 
//			3.04ax:
// 				494 304 8 -5
// 
//			3.04ay:
// 				515 313 8 8
// 
//			3.04az:
// 				536 322 8 5
// 
//			3.04ba:
// 				525 317 8 6
// 
//			3.04bc:
// 				123 266 -2 14
// 
//			3.04be:
// 				99 110 -10 -22
// 
//			3.04bg:
//				383 73 16 -14
// 
//			3.04bh:
//				110 188 -6 -3
// 
//			3.04bh:
//				119 335 0 14
// 
//			3.04bi:
//				490 302 6 11
// 
//			3.04bj:
//				121 266 -2 14
// 
//			3.04bk:
//				402 343 -8 -5
// 
//			3.04bl:
//				500 400 50 -20
// 
//			3.04bm:
//				365 366 -11 -8
// 
//			3.04bn:
//				423 352 -8 -8
// 
//			3.04bo:
//				418 349 -8 -7
// 
//			3.04bp:
//				407 338 -8 -3
// 
//			3.04bq:
//				200 100 -1 -1
// 
//			3.04br:
//				400 200 -5 -10
// 
//			3.04bs:
//				150 100 0 0
// 
//			3.04bt:
//				100 50 5 0
// 
//			3.04bu:
//				257 32 -1 -1
// 
//			3.04bv:
//				125 75 2 -1
// 
//			Mejores versiones:
//				bb  99 111 -10 -17	(+32)
//				aj 122 266  -2  14	(+32)
//				ar 494 304   8  11	(+30)
// 
/////////////////////////////////////////////////

// Fichero log
#define	AV_NO_LOG		0
#define	AV_LOG			1
/////////////////////////////////////////////////
#define	FICHERO_LOG		AV_NO_LOG
/////////////////////////////////////////////////
//#define DEBUG_NNUE
//#define MODO_PARAM

#ifdef _MSC_VER       
	//#define AVX2
	#define AVX512
	//#define SOFT_INTRINSICS
#endif

#if defined(VERS_304)
	#define VERSION "Anubis v3.04"
	#define NOMBRE_LOG "An_3.04_"


											//	1.01	1.02	2.00	2.01	2.02	2.03	2.10	3.00	3.01	3.02	3.03	3.04
											//----------------------------------------------------------------------------------------------
	#define AS_DELTA_FH_MULT 2				//																							   x	// Multiplicador de delta en fail high
	//#define AS_DELTA_FL_MULT 2			//																							   .	// Multiplicador de delta en fail low
	#define AS_DELTA_FL_MULT 3				//																							   x	// Multiplicador de delta en fail low
	#define AS_DELTA_DIN_BASE 10			//	   8																					   x	// El delta se recalcula en cada iteración a partir de la eval; el número determina la base del delta
	#define AS_DELTA_DIN_DIVISOR 8			//	   8																					   x	// Complementa al anterior (DIN es de dinámico)
	//#define BUS_BORRAR_KILLERS			//																							   .

	#define BUS_IID							//																							   x
	#define BUS_IIR_PV 1					//																							   x	// Reducir 1 ply en nodos PV
	#define BUS_IIR_CUT 1					//																							   x	// Reducir 1 ply en nodos cut
	//#define BUS_IIR_CUT_JC 1				//																							   .
	//#define BUS_IIR_CUT 2					//											 -15											   .	// Respecto a BUS_IIR_CUT 1
	#define BUS_IIR_ALL 1					//											 114											   x	// Respecto a quitarlo
	//#define BUS_IIR_ALL_JC 1				//																							   .
	//#define BUS_LMR 1						//																							   .
	//#define BUS_LMR 2						//	  -5	 -28																			   .	// Respecto a BUS_LMR 3
	#define BUS_LMR 3						//																							   x	// BUS_LMR 2 + s32Prof >= 17 && (bEsCutNode || !bMejorando) && u32Legales >= (s32Prof + 5)
	//#define BUS_LMR 4						//											 -53											   .	// Respecto a BUS_LMR 3
	//#define BUS_RED_LEGALES_X_PROF		//																							   .	// En reducciones, limitar legales por prof

	//#define BUS_NM_ADAP 1					//																							   .	// 3 + prof / 5
	//#define BUS_NM_ADAP 2					//																							   .	// 3 + prof / 3 + min
	#define BUS_NM_ADAP 3					//																							   x	// 3 + prof / 3 + min + verif
	//#define BUS_NM_NO_CREO_QUE_PUEDA_FH	//																							   .

	//#define BUS_RAZOR_MARGEN 300			//					 -24																	   .	// Respecto a BUS_RAZOR_MARGEN 400
	//#define BUS_RAZOR_MARGEN 350			//					 -17																	   .	// Respecto a BUS_RAZOR_MARGEN 400
	#define BUS_RAZOR_MARGEN 400			//																							   x
	//#define BUS_RAZOR_MARGEN 450			//					 -12																	   .	// Respecto a BUS_RAZOR_MARGEN 400
	//#define EXT_CAPT_CERCA				//																							   .
	//#define EXT_FINAL						//			 -27																			   .
	//#define EXT_PEON_7					//			 -14																			   .
	//#define EXT_RECAPTURA					//			 -33																			   .
	//#define EXT_SINGULAR 500				//																							   .	// Diferencia para extensión singular doble
	//#define EXT_SINGULAR 100				//	 -16	  13																			   .	// Respecto a EXT_SINGULAR 500
	#define EXT_SINGULAR 50					//			  33																			   x	// Respecto a EXT_SINGULAR 500
	//#define EXT_SINGULAR 25				//	 		   1																			   .	// Respecto a EXT_SINGULAR 500
	//#define HASH_NO_GRABAR_MATES			//																							   .
	//#define HASH_RANURAS 3				//											 -40											   .	// Respecto a HASH_RANURAS 4
	#define HASH_RANURAS 4					//																							   x
	//#define HASH_RANURAS 5				//							 +43	 -25													   .	// Respecto a HASH_RANURAS 4
	#define PODA_FUTIL						//																							   x
	//#define PODA_JC 1						//																							   .
	#define PODA_JC 2						//																							   x
	//#define PODA_JC 3						//			 -47																			   .	// Respecto a PODA_JC 2
	//#define PODA_LMP						//			 -19							-118											   .
	#define PODA_SEE_NEGATIVO 1				//																							   x
	//#define PODA_SEE_NEGATIVO 2			//											-287											   .	// Respecto a PODA_SEE_NEGATIVO 1

	// podas alfa y beta: 10, 185 0 13, 390 30 48																						   x

	#define REFUTACION						//																7							   x

	//#define QS_PODA_DELTA 1000			//							 +17															   .
	#define QS_PODA_DELTA 1050				//							 +48															   x
	//#define QS_PODA_DELTA 1100			//							 +28															   .
	#define QS_PODA_FUTIL 300				//																							   x
	#define QS_PODA_SEE_NEGATIVO			//																							   x
	#define QS_PROF_JAQUES 2				//																							   x
	//#define QS_STANDPAT_AUM 5				//																							   .	// Stand pat aumentado. Idea tomada de Ippolit
	//#define RED_SOSEZ 10					//																							   .
	//#define RED_SOSEZ 15					//																							   .

	//#define PER_EXTDOBLE1SOLOESCAPE		//																							   .
	#define PER_PODARJAQUESSEENEGATIVO		//																							   x	// Sólo con QS_PROF_JAQUES activada
	#define PER_PODARJAQUESPOSPERDIDA		//																							   x	// Sólo con QS_PROF_JAQUES activada
	#define PER_MEMORIAHASH 350				//																							   x

	//#define QS_COMPROBAR_PROGRESO_JAQUES
	//#define QS_INTENTAR_MATES_EN_QSNORMAL

#endif // #if defined(VERS_xx)

/*
=== Correlaciones con Corte ===
Corte                    1.000000	// Es lo que me indica que la reducción es correcta, porque se produce el mismo corte en la búsqueda reducida y en la nominal
Ev_amenaza               0.064362
Mejorando                0.050686
abs_eval                 0.043056
abs_ev_ajust             0.043021
ajust_geq_beta_mej       0.028623	// eval ajustada >= beta y mejorando
ajust_geq_beta           0.023138	// eval ajustada >= beta
jugHash                  0.015102	// hay jugada hash
Eval                     0.009253
Ev_ajust                 0.008771	// eval - amenaza
mej_and_notHash          0.008257
ajust_leq_alfa          -0.023138	// eval ajustada <= alfa
Alfa                    -0.026115
Beta                    -0.026115
ajust_leq_alfa_notMej   -0.057611	// eval ajustada <= alfa y no mejorando
Name: Corte, dtype: float64

=== Árbol de decisión ===
|--- abs_ev_ajust <= 10.50
|   |--- Alfa <= -0.50
|   |   |--- ajust_leq_alfa_notMej <= 0.50
|   |   |   |--- Beta <= -7.50
|   |   |   |   |--- class: 1
|   |   |   |--- Beta >  -7.50
|   |   |   |   |--- class: 1
|   |   |--- ajust_leq_alfa_notMej >  0.50
|   |   |   |--- Eval <= -2.50
|   |   |   |   |--- class: 0
|   |   |   |--- Eval >  -2.50
|   |   |   |   |--- class: 1
|   |--- Alfa >  -0.50
|   |   |--- Alfa <= 8.50
|   |   |   |--- Beta <= 2.50
|   |   |   |   |--- class: 1
|   |   |   |--- Beta >  2.50
|   |   |   |   |--- class: 1
|   |   |--- Alfa >  8.50
|   |   |   |--- Eval <= 50.00
|   |   |   |   |--- class: 1
|   |   |   |--- Eval >  50.00
|   |   |   |   |--- class: 1
|--- abs_ev_ajust >  10.50
|   |--- Eval <= 31.50
|   |   |--- Alfa <= 0.50
|   |   |   |--- Eval <= 9.50
|   |   |   |   |--- class: 1
|   |   |   |--- Eval >  9.50
|   |   |   |   |--- class: 1
|   |   |--- Alfa >  0.50
|   |   |   |--- abs_eval <= 14.50
|   |   |   |   |--- class: 1
|   |   |   |--- abs_eval >  14.50
|   |   |   |   |--- class: 1
|   |--- Eval >  31.50
|   |   |--- Alfa <= 36.50
|   |   |   |--- abs_eval <= 47.50
|   |   |   |   |--- class: 1
|   |   |   |--- abs_eval >  47.50
|   |   |   |   |--- class: 1
|   |   |--- Alfa >  36.50
|   |   |   |--- abs_eval <= 46.50
|   |   |   |   |--- class: 1
|   |   |   |--- abs_eval >  46.50
|   |   |   |   |--- class: 1


=== Importancia de variables (RandomForest) ===
Alfa                     0.220509
Beta                     0.217297
abs_ev_ajust             0.114957
abs_eval                 0.107806
Eval                     0.104509
Ev_ajust                 0.101788
jugHash                  0.036514
Ev_amenaza               0.024731
ajust_leq_alfa           0.016693
ajust_geq_beta           0.013147
ajust_leq_alfa_notMej    0.012793
Mejorando                0.011447
mej_and_notHash          0.010030
ajust_geq_beta_mej       0.007780
dtype: float64
C:\Users\JOSE CARLOS\AppData\Local\Programs\Python\Python313\Lib\site-packages\sklearn\linear_model\_logistic.py:465: ConvergenceWarning: lbfgs failed to converge (status=1):
STOP: TOTAL NO. OF ITERATIONS REACHED LIMIT.

Increase the number of iterations (max_iter) or scale the data as shown in:
    https://scikit-learn.org/stable/modules/preprocessing.html
Please also refer to the documentation for alternative solver options:
    https://scikit-learn.org/stable/modules/linear_model.html#logistic-regression
  n_iter_i = _check_optimize_result(

=== Coeficientes regresión logística ===
Alfa                 -0.443224
Beta                  0.431316
Eval                  0.005041
Ev_amenaza            0.010086
jugHash               0.500418
Mejorando             0.305131
Ev_ajust             -0.005045
ajust_leq_alfa        0.209335
ajust_geq_beta        0.665205
ajust_leq_alfa_notMej 0.230142
ajust_geq_beta_mej    0.325938
mej_and_notHash       0.155119
abs_eval              0.013375
abs_ev_ajust         -0.013361

=== Reporte de clasificación (RandomForest) ===
              precision    recall  f1-score   support

           0       0.72      0.38      0.50     19704
           1       0.97      0.99      0.98    377336

    accuracy                           0.96    397040
   macro avg       0.84      0.69      0.74    397040
weighted avg       0.96      0.96      0.96    397040

-> Se comprueba que no hay correlación entre los valores disponibles y el resultado que se desea predecir (si la búsqueda reducida y la nominal producen el mismo corte)
*/