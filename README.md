Esta historia empieza a principios de los 2000.

Durante los siguientes años, casi no hago nada en el programa, hasta llegar a 2024, que encuentro los fuentes por casualidad y trato de modernizar un poco el programa.

Verás que la estructura es anticuada, la implementación ineficiente y algunas cosas que he incluido recientemente, parecen hechas por otra persona. Han pasado más de 20 años y hasta yo evoluciono.

He subido versiones antiguas, para aquellas personas que gustan de coleccionar programas de ajedrez en sus diferentes (y muchas veces plagadas de errores) versiones a lo largo de los años.

Tengo pendiente habilitar la posibilidad de modificar el tamaño de las tablas hash y, al final, convertir Anubis en un motor UCI, con todas las ventajas que ello conlleva para el usuario a efectos de configuración. El problema es que tengo tan poco tiempo libre, y tantas ideas para mejorar la fuerza de juego, que esos cambios los voy retrasando y no acaban de llegar nunca.

Pido disculpas por las incomodidades y espero que, al menos, la originalidad del programa (con todos sus defectos y arcaísmos) sea un aliciente.



Sólo tengo windows y sólo tengo MSVS y sólo sé compilar para AVX2 o AVX512.

No soy un profesional de la programación en C (me dedico profesionalmente a las bases de datos) y eso se nota en mi estilo de programación y en las limitaciones de compilación.

Por cierto, no sé nada de instrucciones AVX. Todo el código he tenido que hacerlo con diversas IAs (ninguna me lo hacía bien a la primera). Nunca habría sido capaz de implementar estas mejoras, y ni siquiera hubiera sido capaz de implementar NNUE, si no hubiesen extistido estos modelos de IA capaces de generar código y explicarte cómo funcionan cosas complejísimas.



Y ahora que hablo de NNUE: intenté la cuantización y no mejoraba nada el rendimiento en mi ordenador (quizá en otros sí, pero sólo tengo el mío) y lo descarté porque aumentaba la complejidad si aportar nada a cambio.

También intenté implementar acumuladores incrementales. Me costó una eternidad hacerlos funcionar, porque las IAs se empeñaban en decirme que iba a mejorar el rendimiento dramáticamente... Pues tampoco. Así que lo descarté y me quedé con mi implementación burda y sencilla.

También probé una infinidad de arquetecturas de red, con varias redes o con una sola, con más o menos capas más o menos grandes... La inmensa mayoría de las cosas que probé funcionaron fatal, pero al menos aprendí algo por el camino y entendí un poquito como funciona una red neuronal.



Agradecimientos:

A todos los autores que han publicado su código fuente, sin importar la fuerza del programa. Sin esa información, yo no habría sido capaz de implementar ni el "Hello world".

A todos los autores que publican sus ejecutables, sin importar la fuerza del programa. Todos son útiles para probar mis versiones y para entender cómo funcionan los programas de ajedrez.

A todas las personas que hacen test y listas de elo y revisiones de cualquier tipo. Siempre es un aliciente leer lo que opinan otros sobre tu trabajo, especialmente las críticas constructivas.

A las personas que me ayudan proponiendo código que mejora el mío o arregla bugs.



Siento no poder contestar rápido a todo el mundo. No tengo mucho tiempo y, además, no estoy en mi mejor momento de salud. Así que... paciencia.

