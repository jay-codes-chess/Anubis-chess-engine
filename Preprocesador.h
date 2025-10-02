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
#define VERS_306
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
//			3.06_:
//				A	BUS_IID		(S/N)
//				B	BUS_IIR_PV	[0..4]
//				C	BUS_IIR_CUT	[0..4]
//				D	BUS_IIR_ALL	[0..4]
//
//				1 1 1 1
// 
// 			3.06a001: (-10)
//				0 1 1 1
// 
// 			3.06a002: (+11)
//				0 0 1 1
// 
// 			3.06a003: (-14)
//				0 0 0 1
// 
// 			3.06a004: (-49)
//				0 0 0 0
// 
// 			3.06a005: (-34)
//				1 0 0 0
// 
// 			3.06a006: (-88)
//				0 1 0 0
// 
// 			3.06a007: (-13)
//				0 0 1 0
// 
// 			3.06a008: (-63)
//				0 1 1 0
// 
// 			3.06a009: (+14)
//				1 0 0 1
// 
// 			3.06a010: (-3)
//				0 1 2 1
// 
// 			3.06a011: (+27)
//				1 0 1 1
// 
// 			3.06a012: (-34)
//				0 2 3 2
// 
// 			3.06a013: (-19)
//				0 2 3 2
// 
// 			3.06a014: (-56)
//				1 2 1 1
// 
// 			3.06a015: (+8)
//				1 1 1 2
// 
// 			3.06a016: (-5)
//				1 0 1 0
// 
// 			3.06a017: (-83)
//				1 1 0 0
// 
// 			3.06a018: (0)
//				1 1 1 3
// 
// 			3.06a019: (-75)
//				1 1 1 0
// 
// 			3.06a020: (+20)
//				1 0 1 2
// 
// 			3.06a021: (+28)		****************************************************************************
//				1 0 3 1			****************************************************************************
// 
// 			3.06a022: (+25)
//				1 0 2 1
// 
/////////////////////////////////////////////////

// Fichero log
#define	AV_NO_LOG		0
#define	AV_LOG			1
/////////////////////////////////////////////////
#define	FICHERO_LOG		AV_NO_LOG
/////////////////////////////////////////////////
//#define DEBUG_NNUE

#ifdef _MSC_VER       
	//#define AVX2
	#define AVX512
	//#define SOFT_INTRINSICS
#endif

#if defined(VERS_306)
	#define VERSION "Anubis v3.06"
	#define NOMBRE_LOG "An_3.06_"


											//	1.02	2.00	2.01	2.02	2.03	2.10	3.00	3.01	3.02	3.03	3.04
											//----------------------------------------------------------------------------------------------
	#define AS_DELTA_FH_MULT 2				//																					   x	// Multiplicador de delta en fail high
	//#define AS_DELTA_FL_MULT 2			//																					   .	// Multiplicador de delta en fail low
	#define AS_DELTA_FL_MULT 3				//																					   x	// Multiplicador de delta en fail low
	#define AS_DELTA_DIN_BASE 10			//																					   x	// El delta se recalcula en cada iteración a partir de la eval; el número determina la base del delta
	#define AS_DELTA_DIN_DIVISOR 8			//																					   x	// Complementa al anterior (DIN es de dinámico)
	//#define BUS_BORRAR_KILLERS			//																					   .

	// 1 0 3 1								// 3.06: +28
	#define BUS_IID							//																					   x
	#define BUS_IIR_PV 0					//																					   x	// Reducir x ply en nodos PV
	#define BUS_IIR_CUT 3					//																					   x	// Reducir x ply en nodos cut
	#define BUS_IIR_ALL 1					//																					   x	// Reducir x ply en nodos all

	//#define BUS_LMR 1						//																					   .
	//#define BUS_LMR 2						//	 -28																			   .	// Respecto a BUS_LMR 3
	#define BUS_LMR 3						//																					   x	// BUS_LMR 2 + s32Prof >= 17 && (bEsCutNode || !bMejorando) && u32Legales >= (s32Prof + 5)
	//#define BUS_LMR 4						//									 -53											   .	// Respecto a BUS_LMR 3
	//#define BUS_RED_LEGALES_X_PROF		//																					   .	// En reducciones, limitar legales por prof

	//#define BUS_NM_ADAP 1					//																					   .	// 3 + prof / 5
	//#define BUS_NM_ADAP 2					//																					   .	// 3 + prof / 3 + min
	#define BUS_NM_ADAP 3					//																					   x	// 3 + prof / 3 + min + verif
	//#define BUS_NM_NO_CREO_QUE_PUEDA_FH	//																					   .

	//#define BUS_RAZOR_MARGEN 300			//			 -24																	   .	// Respecto a BUS_RAZOR_MARGEN 400
	//#define BUS_RAZOR_MARGEN 350			//			 -17																	   .	// Respecto a BUS_RAZOR_MARGEN 400
	#define BUS_RAZOR_MARGEN 400			//																					   x
	//#define BUS_RAZOR_MARGEN 450			//			 -12																	   .	// Respecto a BUS_RAZOR_MARGEN 400
	//#define EXT_CAPT_CERCA				//																					   .
	//#define EXT_FINAL						//	 -27																			   .
	//#define EXT_PEON_7					//	 -14																			   .
	//#define EXT_RECAPTURA					//	 -33																			   .
	//#define EXT_SINGULAR 500				//																					   .	// Diferencia para extensión singular doble
	//#define EXT_SINGULAR 100				//	  13																			   .	// Respecto a EXT_SINGULAR 500
	#define EXT_SINGULAR 50					//	  33																			   x	// Respecto a EXT_SINGULAR 500
	//#define EXT_SINGULAR 25				//	   1																			   .	// Respecto a EXT_SINGULAR 500
	//#define HASH_NO_GRABAR_MATES			//																					   .
	//#define HASH_RANURAS 3				//									 -40											   .	// Respecto a HASH_RANURAS 4
	#define HASH_RANURAS 4					//																					   x
	//#define HASH_RANURAS 5				//					 +43	 -25													   .	// Respecto a HASH_RANURAS 4
	#define PODA_FUTIL						//																					   x
	//#define PODA_JC 1						//																					   .
	#define PODA_JC 2						//																					   x
	//#define PODA_JC 3						//	 -47																			   .	// Respecto a PODA_JC 2
	//#define PODA_LMP						//	 -19							-118											   .
	#define PODA_SEE_NEGATIVO 1				//																					   x
	//#define PODA_SEE_NEGATIVO 2			//									-287											   .	// Respecto a PODA_SEE_NEGATIVO 1

	// poda hash beta: 185 0 13 0
	// poda hash alfa: 99 111 -10 -17

	#define REFUTACION						//															7							   x

	//#define QS_PODA_DELTA 1000			//						 +17															   .
	#define QS_PODA_DELTA 1050				//						 +48															   x
	//#define QS_PODA_DELTA 1100			//						 +28															   .
	#define QS_PODA_FUTIL 300				//																						   x
	#define QS_PODA_SEE_NEGATIVO			//																						   x
	#define QS_PROF_JAQUES 2				//																						   x
	//#define QS_STANDPAT_AUM 5				//																						   .	// Stand pat aumentado. Idea tomada de Ippolit
	//#define RED_SOSEZ 10					//																						   .
	//#define RED_SOSEZ 15					//																						   .

	//#define PER_EXTDOBLE1SOLOESCAPE		//																						   .
	#define PER_PODARJAQUESSEENEGATIVO		//																						   x	// Sólo con QS_PROF_JAQUES activada
	#define PER_PODARJAQUESPOSPERDIDA		//																						   x	// Sólo con QS_PROF_JAQUES activada
	#define PER_MEMORIAHASH 350				//																						   x

	//#define QS_COMPROBAR_PROGRESO_JAQUES
	//#define QS_INTENTAR_MATES_EN_QSNORMAL

#endif // #if defined(VERS_xx)

