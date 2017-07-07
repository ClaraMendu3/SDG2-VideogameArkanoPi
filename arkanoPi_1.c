#include "arkanoPi_1.h"

//Comentar y descomentar para mostrar (o no mostrar) mensajes de depuración por pantalla. Comentar ahorra recursos y da fluidez al refresco de leds
//#define __MODO_DEBUG_TERMINAL__

static  tipo_juego juego; //volatile

//Declaramos esta variable global para que el jugador introduzca la velocidad de juego deseada por teclado.
//Este valor ser¨¢ el "reloj" de la pelota y lo usaremos en tmr_startms(juego.TIMER2, velocidad);
int velocidad;

//Declaramos esta variable global para poder disponer de varias vidas en el juego. Tendremos 3 vidas!
int numeroVidas;

//Array para tener ordenados los pines de las filas (pues los pines software no van seguidos, son: 0,1, 2, 3, 4, 7,  23)
//para poder utilizar un bucle for y un ¨ªndice a la hora de iluminar...
int matrizPinesFilas[7]={pinFila0, pinFila1, pinFila2,pinFila3, pinFila4, pinFila5, pinFila6};

//Variable global y volatile de 32 bits que contiene todos los FLAGS del juego (FLAG_PELOTA, FLAG_RAQUETA_DERECHA, FLAG_FINAL_JUEGO...)
//Es volatile porque diferentes recursos pueden acceder a ella (previo bloqueo) y modificar su valor.
volatile int flags = 0;

//Variable global utilizada para el refresco/iluminaci¨®n de columnas. Empezamos por la columna 0!
int columna=0;

//Variable global para el funcionamiento del Joystick.  Queremos almacenar la ¨²ltima lectura para comparar si la actualizar
//es mayor o menor y de acuerdo a eso desplazar la posici¨®n de la raqueta. Iniciamos a 3 para centrar la raqueta...
//Tener en cuenta que juego.arkanoPi.raqueta.x es el extremo izquierdo de la misma!
int lecturaAnterior=3;

//***********************************************************//
//MEJORA DEL PARPADEO tras rebote con raqueta//

//FUNCIONAMIENTO: la pelota debe parpadear durante el segundo inmediatamente posterior al rebote con la raqueta.
//El estado de encendido/apagado del led se debe actualizar al menos 1 vez cada 200 milisegundos.
//Apaga --> 200ms --> Enciende --> 200ms --> Apaga --> 200ms --> Enciende --> 200ms --> Apaga --> 200ms --> Enciende... SUMA 1s!
//El primer apagado lo hacemos directamente en el metodo ReboteRaqueta, asi que el IF de apagaPelota se tiene que ejecutar 4 veces,
//por lo tanto, como iniciamos nParpadeos a 0, se tiene que ejecutar cuando nParpadeos valga 0,1,2,4,5 asi que ponemos la condicion <5.

int nParpadeos=0;

void enciendePelota (union sigval value) {

	if(juego.arkanoPi.pelota.y !=6){
	 juego.arkanoPi.pantalla.matriz[juego.arkanoPi.pelota.x][juego.arkanoPi.pelota.y] = 1;

	 #ifdef __MODO_DEBUG_TERMINAL__
	 piLock (STD_IO_BUFFER_KEY);
	 printf("Está en enciendePelota! \n");
	 PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
	 piUnlock (STD_IO_BUFFER_KEY);
	 #endif

         tmr_startms(juego.TIMER_APAGA_PELOTA, CLK_PARPADEO_PELOTA);}
}

void apagaPelota(union sigval value) {

	if(nParpadeos<5){
	 juego.arkanoPi.pantalla.matriz[juego.arkanoPi.pelota.x][juego.arkanoPi.pelota.y] = 0;
	 nParpadeos++;

	 tmr_startms(juego.TIMER_ENCIENDE_PELOTA, CLK_PARPADEO_PELOTA);}

	else {
         nParpadeos=0;
	 tmr_stop(juego.TIMER_ENCIENDE_PELOTA);
	 tmr_stop(juego.TIMER_APAGA_PELOTA);
	 tmr_destroy (juego.TIMER_ENCIENDE_PELOTA);
	 tmr_destroy (juego.TIMER_APAGA_PELOTA);

	 #ifdef __MODO_DEBUG_TERMINAL__
	 piLock (STD_IO_BUFFER_KEY);
	 printf("Esta en apagaPelota! \n");
	 PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
	 piUnlock (STD_IO_BUFFER_KEY);
	 #endif
	}
}
//***********************************************************//

//***********************************************************//
//MEJORA CAMBIO VELOCIDAD//
int limiteVelocidadMinima=600; //AJUSTAR!!!!

void cambiarVelocidad(void){

	if(velocidad>= limiteVelocidadMinima +10*velocidad/100){ //Debe ser mayor o igual al 10% del limite para poder decrementar
	 velocidad = velocidad - 10*velocidad/100;

         #ifdef __MODO_DEBUG_TERMINAL__
	 piLock (STD_IO_BUFFER_KEY);
	 printf("\n\n Ha cambiado velocidad, new velocidad: %d !!!\n\n", velocidad);
	 piUnlock (STD_IO_BUFFER_KEY);
	 #endif

	}else{;}
}
//***********************************************************//

//Función de retardo.
// espera hasta la próxima activación del reloj
void delay_until (unsigned int next) {
	unsigned int now = millis();

	if (next > now) {
		delay (next - now);
    }
}

//------------------------------------------------------
// FUNCIONES DE ACTIVACI¨®N DE LOS PULSADORES. Activamos los flags de los pulsadores Hay que tener cuidado con los rebotes. 
//------------------------------------------------------
//Activamos los flags de los pulsadores al detectarse en una pulsación a través del pin correspondiente.  (Una interrupción consistente en un flanco de bajada!)
//Hay que tener cuidado con los rebotes. De ah¨ª el uso de DEBOUNCE_TIME, debounceTime, delay, millis...

void activaFlagPulsadorDerecho(){
	// Pin event (key / button event) debouncing procedure
	// "millis()" returns a number representing the number of milliseconds since our
	// program called one of the wiringPiSetup functions (wiringPiSetupGpio in our case).
	int debounceTime =0;

	if (millis () < debounceTime) {
	 debounceTime = millis () + DEBOUNCE_TIME ;
	 return;
	}

	// Atención a la interrupción
	piLock (FLAGS_KEY);
	flags |= FLAG_RAQUETA_DERECHA;
	flags |= FLAG_TECLA;
        piUnlock (FLAGS_KEY);

        // Wait for key to be released
        while (digitalRead (pinPulsadorIzquierdo) == HIGH) {
         delay (1) ;
        }
       debounceTime = millis () + DEBOUNCE_TIME ;
}

void activaFlagPulsadorIzquierdo(){
	int debounceTime =0;

	// Pin event (key / button event) debouncing procedure
	// "millis()" returns a number representing the number of milliseconds since our
	// program called one of the wiringPiSetup functions (wiringPiSetupGpio in our case).
	if (millis () < debounceTime) {
	 debounceTime = millis () + DEBOUNCE_TIME ;
	 return;
	}

	// Atención a la interrupci¨®n
        piLock (FLAGS_KEY);
	flags |= FLAG_RAQUETA_IZQUIERDA;
	flags |= FLAG_TECLA;
	piUnlock (FLAGS_KEY);

	// Wait for key to be released
	while (digitalRead (pinPulsadorDerecho) == HIGH) {
	 delay (1) ;
	}
	debounceTime = millis () + DEBOUNCE_TIME ;
}

void activaFlagPulsadorStop(){
	// Pin event (key / button event) debouncing procedure
	// "millis()" returns a number representing the number of milliseconds since our
	// program called one of the wiringPiSetup functions (wiringPiSetupGpio in our case).
	int debounceTime =0;

	if (millis () < debounceTime) {
	 debounceTime = millis () + DEBOUNCE_TIME ;
	 return;
	}

	// Atención a la interrupci¨®n
	piLock (FLAGS_KEY);
	flags |= FLAG_STOP;
	flags |= FLAG_TECLA;
        piUnlock (FLAGS_KEY);

       // Wait for key to be released
       while (digitalRead (pinPulsadorIzquierdo) == HIGH) {
        delay (1) ;
       }
       debounceTime = millis () + DEBOUNCE_TIME ;
}

//Periódicamente activamos FLAG_PELOTA para desplazar la pelota según la trayectoria correspondiente.
//Activamos FLAG_JOYSTICK por primera vez al crear y consumirse el tiempo indicado por CLK_PELOTA al iniciar el temporizador en el main.
//El temporizador se vuelve a iniciar de nuevo tras activar el flag con tmr_startms(juego.TIMER2, velocidad) para conseguir esta periodicidad y que al consumirse el tiempo
//volvamos a esta funci¨®n de  nuevo para activar el flag!
void activaFlagPelota(union sigval value){
	piLock (FLAGS_KEY);
	flags |= FLAG_PELOTA;
	flags |= FLAG_TECLA;
	piUnlock (FLAGS_KEY);

	tmr_startms(juego.TIMER2, velocidad);
}

//Periódicamente activamos FLAG_JOYSTICK para que se compruebe el valor del mismo y desplazar la raqueta
//Activamos FLAG_JOYSTICK por primera vez al crear y consumirse el tiempo indicado por CLK_JOYSTICK al iniciar el temporizador en InicializaJuego.
//El temporizador se vuelve a iniciar de nuevo tras activar el flag tmr_startms(juego.TIMER_JOYSTICK, CLK_JOYSTICK) para conseguir esta periodicidad y que al consumirse el tiempo
//volvamos a esta función de  nuevo para activar el flag!
void activaFlagJoystick(union sigval value){
	piLock (FLAGS_KEY);
	flags |= FLAG_JOYSTICK;
        piUnlock (FLAGS_KEY);

	tmr_startms(juego.TIMER_JOYSTICK, CLK_JOYSTICK);
}

//------------------------------------------------------
// FUNCIÓN DE DESACTIVACIÓN SONIDO. Al poner la frecuencia a cero paras el sonido!
//------------------------------------------------------
void desactivaSonido(union sigval value){
	softToneWrite(pinSonido, 0);
}

//------------------------------------------------------
// FUNCIONES DE LA MAQUINA DE ESTADOS
//------------------------------------------------------

// FUNCIONES DE ENTRADA O COMPROBACI¨®N DE LA MAQUINA DE ESTADOS
//Se comprueba el valor del flag correspondiente y si vale 1, se produce una transici¨®n en la m¨¢quina de estados
//y se lanza la funci¨®n de salida asociada.

int compruebaTeclaPulsada (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_TECLA);
	piUnlock (FLAGS_KEY);

	return result;
}

int compruebaTeclaPelota (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_PELOTA);
	piUnlock (FLAGS_KEY);

	return result;
}

int compruebaTeclaRaquetaDerecha (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_RAQUETA_DERECHA);
	piUnlock (FLAGS_KEY);

	return result;
}


int compruebaTeclaRaquetaIzquierda (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_RAQUETA_IZQUIERDA);
	piUnlock (FLAGS_KEY);

	return result;
}

int compruebaTeclaStop (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_STOP);
	piUnlock (FLAGS_KEY);

	return result;
}

int compruebaFinalJuego (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_FINAL_JUEGO);
	piUnlock (FLAGS_KEY);

	return result;
}

int compruebaJoystick (fsm_t* this) {
	int result;

	piLock (FLAGS_KEY);
	result = (flags & FLAG_JOYSTICK);
	piUnlock (FLAGS_KEY);

	return result;
}

//------------------------------------------------------
// FUNCIONES DE ACCION. Debemos desactivar el flag correspondiente!!!
//------------------------------------------------------

// void InicializaJuego (void): funcion encargada de llevar a cabo
// la oportuna inicializaci¨®n de toda variable o estructura de datos
// que resulte necesaria para el desarrollo del juego.
void InicializaJuego (fsm_t* this) {
	// A completar por el alumno... OKK
	numeroVidas=3;  

        piLock (STD_IO_BUFFER_KEY);
	printf("Escriba un valor mayor que 700 para darle la velocidad deseada al movimiento de la pelota. Cuanto mayor es el valor, menor es la velocidad! \n");
	piUnlock (STD_IO_BUFFER_KEY);
	scanf("%d", &velocidad);
	
	InicializaArkanoPi(&(juego.arkanoPi));
	
	//Creamos e inicializamos el temporizador de la pelota, del sonido y del joystick aqu¨ª en vez de en el main pues hasta este punto no es utilizado.
	//No tiene sentido configuarlo antes de tiempo. No ocurre lo mismo con el temporizador encargado de el refresco de la matriz de leds
	//que necesitamos su creaci¨®n desde el principio.
	//La función que pones entre par¨¦ntesis de la sentencia juego.TIMERX= tmr_new (Y) es la encargada de actuar al consumirse el CLK_Z correspondiente!!!
	juego.TIMER2= tmr_new (activaFlagPelota);
	tmr_startms(juego.TIMER2, velocidad);
	
	juego.TIMER_SONIDO= tmr_new (desactivaSonido);

        juego.TIMER_JOYSTICK= tmr_new (activaFlagJoystick);
        tmr_startms(juego.TIMER_JOYSTICK, CLK_JOYSTICK);
}

//Tener en cuenta que la pala siempre debe tener un cuadradito dentro de la pantalla y su X nos indica el extremo izquierdo!!!!!!!!!!!!!

// void MueveRaquetaIzquierda (void): funcion encargada de ejecutar
// el movimiento hacia la izquierda contemplado para la raqueta.
// Debe garantizar la viabilidad del mismo mediante la comprobaci¨®n
// de que la nueva posición correspondiente a la raqueta no suponga
// que esta rebase o exceda los límites definidos para el área de juego
// (i.e. al menos uno de los leds que componen la raqueta debe permanecer
// visible durante todo el transcurso de la partida).
void MueveRaquetaIzquierda (fsm_t* this) {
	// A completar por el alumno... OKK

	piLock (FLAGS_KEY);
        flags &= ~FLAG_RAQUETA_IZQUIERDA;
	flags &= ~FLAG_TECLA;
	piUnlock (FLAGS_KEY);

	if(juego.arkanoPi.raqueta.x == -2 ){
	 juego.arkanoPi.raqueta.x =-2;
	}
	else{
	 juego.arkanoPi.raqueta.x -=1;
	}

	ActualizaPantalla(&(juego.arkanoPi));
	
	#ifdef __MODO_DEBUG_TERMINAL__ 
	piLock (STD_IO_BUFFER_KEY);
        PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
	piUnlock (STD_IO_BUFFER_KEY);
	#endif
}

// void MueveRaquetaDerecha (void): funci¨®n similar a la anterior
// encargada del movimiento hacia la derecha.
void MueveRaquetaDerecha (fsm_t* this) {
	// A completar por el alumno... OKK

	piLock (FLAGS_KEY);
	flags &= ~FLAG_RAQUETA_DERECHA;
        flags &= ~FLAG_TECLA;
        piUnlock (FLAGS_KEY);

	if(juego.arkanoPi.raqueta.x == 9 ){
	 juego.arkanoPi.raqueta.x =9;
	}

	else{
	 juego.arkanoPi.raqueta.x +=1;
	}

	ActualizaPantalla(&(juego.arkanoPi));
	
	#ifdef __MODO_DEBUG_TERMINAL__ 
	piLock (STD_IO_BUFFER_KEY);
	PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
	piUnlock (STD_IO_BUFFER_KEY);
	#endif
}

// void MovimientoPelota (void): función encargada de actualizar la
// posición de la pelota conforme a la trayectoria definida para esta.
// Para ello deber¨ªa identificar los posibles rebotes de la pelota para,
// en ese caso, modificar su correspondiente trayectoria (los rebotes
// detectados contra alguno de los ladrillos implicaría adicionalmente
// la eliminación del ladrillo). Del mismo modo, debería también
// identificar las situaciones en las que se da por finalizada la partida:
// bien porque el jugador no consiga devolver la pelota, y por tanto esta
// rebase el límite inferior del área de juego, bien porque se agoten
// los ladrillos visibles en el área de juego.
void MovimientoPelota (fsm_t* this) {
	// A completar por el alumno... OKK

	piLock (FLAGS_KEY);
	flags &= ~FLAG_PELOTA;
	flags &= ~FLAG_TECLA;
	piUnlock (FLAGS_KEY);

	//No has conseguido  golpear la pelota con la raqueta...
	if(juego.arkanoPi.pelota.y==6){

	 //Decrementamos el número de vidas
	 numeroVidas--;

         #ifdef __MODO_DEBUG_TERMINAL__ 
	 piLock (STD_IO_BUFFER_KEY);
	 printf("\n\n Ha perdido una vida, le quedan %d vidas, puede seguir jugando!!!\n\n", numeroVidas);
	 piUnlock (STD_IO_BUFFER_KEY);
	 #endif		

         //Si pierdes todas las vidas muestras una carita triste en la matriz de leds y posteriormente vas al estado de FinalJuego
	 if(numeroVidas==0){
	  PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &cero);		
	  delay(1500);
		
	  piLock (FLAGS_KEY);
	  flags|= FLAG_FINAL_JUEGO;
	  piUnlock (FLAGS_KEY);		
	 }

	 //Si te quedan vidas disponibles, muestra el n¨²mero de vidas disponibles en la matriz de leds y posteriormente
	 //colocas pelota y raqueta en la posici¨®n inicial pero manteniendo la situaci¨®n de los ladrillos.
	 else{
			
	  if(numeroVidas==2){
	   PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &dos);		
	   delay(1500);				
	  }
			
	  else if (numeroVidas==1){
	   PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &uno);		
           delay(1500);
	  }
			
	  //Hemos creado este método para Resetear la pelota y la raqueta pero mantener la situación de los ladrillos.	
	  ReinicializaArkanoPiVidas(&(juego.arkanoPi));		
		
	  #ifdef __MODO_DEBUG_TERMINAL__ 
	  piLock (STD_IO_BUFFER_KEY);
	  PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
	  piUnlock (STD_IO_BUFFER_KEY);
	  #endif
         }
	}

	//REBOTES CON RAQUETA
	if (juego.arkanoPi.pelota.y==5){
	 ReboteRaqueta ();
	}

	//REBOTES CON PARED: solo hay que modificar la variaci¨®n de la X
	if((juego.arkanoPi.pelota.x==0 && juego.arkanoPi.pelota.xv==-1 ) || (juego.arkanoPi.pelota.x==9 && juego.arkanoPi.pelota.xv==1) ){
         RebotePared();
	}
	//REBOTES CON LADRILLOS: solo hay que modificar la variaci¨®n de la Y y borrar ladrillo
	if((juego.arkanoPi.pelota.y==2 || juego.arkanoPi.pelota.y==1) && juego.arkanoPi.pelota.yv==-1){
	 ReboteLadrillos();
	}

	//REBOTES CON PARED SUPERIOR: solo hay que modificar la variaci¨®n de la Y
	if(juego.arkanoPi.pelota.y==0 && juego.arkanoPi.pelota.yv==-1){
	 ReboteTecho();
	}

	//Una vez que hemos indicado la nueva trayectoria de la pelota con los if's anteriores teniendo en cuenta la posici¨®n y la trayectoria
        //anteriores, finalmente actualizamos la posici¨®n con el m¨¦todo  auxiliar ActualizaPosicionPelota.	
	ActualizaPosicionPelota ();

	ActualizaPantalla(&(juego.arkanoPi));
	
	#ifdef __MODO_DEBUG_TERMINAL__ 
	piLock (STD_IO_BUFFER_KEY);
	PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
        piUnlock (STD_IO_BUFFER_KEY);
	#endif
}

//MEJORA: Joystick
//void ControlJoystick(void): Utilizamos la lectura del ADC, que se encarga de transformar el valor del potenci¨®metro/joystick de anal¨®gico a digital.
//Tenemos que jugar con el rango de valores digitales que obtenemos para determinar la nueva posici¨®n.
//ByteSPI[1] en nuestro caso nos da valores entre 0 y 63
//salida_SPI en nuestro caso nos  da valores entre 0 y 1984
//Hay que tener en cuenta que tenemos 12 posiciones, es decir, juego.arkanoPi.raqueta.x puede tomar valores entre -2 y 9.
void ControlJoystick (fsm_t* this) {

	piLock (FLAGS_KEY);
	flags &= ~FLAG_JOYSTICK;
	piUnlock (FLAGS_KEY);

	unsigned char ByteSPI[3]; //Buffer lectura escritura SPI
        int resultado_SPI = 0; //Control operacion SPI
        ByteSPI[0] = 0b10011111; // Configuracion ADC (10011111 unipolar, 0-2.5v, canal 0, salida 1), bipolar 0b10010111
        ByteSPI[1] = 0b0;
        ByteSPI[2] = 0b0;
        resultado_SPI = wiringPiSPIDataRW (SPI_ADC_CH, ByteSPI, 3); //Enviamos y leemos tres bytes (8+12+4 bits)
        usleep(20);
        int salida_SPI = ((ByteSPI[1] << 6) | (ByteSPI[2] >> 2)) & 0xFFF;
    
	//OPCIÓN 1: ÚLTIMA LECTURA SALIDA_SPI
        if(lecturaAnterior>salida_SPI + 15){
    	 if(juego.arkanoPi.raqueta.x == -2 ){
    	  juego.arkanoPi.raqueta.x =-2;
    	 }
    	 else{
    	  juego.arkanoPi.raqueta.x -=1;
         }
        }

        else if (lecturaAnterior<salida_SPI - 15){
    	 if(juego.arkanoPi.raqueta.x == 9 ){
    	  juego.arkanoPi.raqueta.x =9;
    	 }
    	 else{
    	  juego.arkanoPi.raqueta.x +=1;
    	 }
        }
        lecturaAnterior= salida_SPI;
	
	
    //OPCIÓN 2: ÚLTIMA LECTURA ByteSPI[2]
    /*if(lecturaAnterior> (ByteSPI[2] >> 3)){
    	if(juego.arkanoPi.raqueta.x == -2 ){
    	 juego.arkanoPi.raqueta.x =-2;
    	}
    	else{
    	 juego.arkanoPi.raqueta.x -=1;
    	}
      }
      else if (lecturaAnterior<(ByteSPI[2] >> 3)){
       if(juego.arkanoPi.raqueta.x == 9 ){
    	juego.arkanoPi.raqueta.x =9;
       }
       else{
    	juego.arkanoPi.raqueta.x +=1;
       }
    }
    lecturaAnterior= (ByteSPI[2] >> 3);*/	

    //OPCIÓN 3: RANGOS con ByteSPI [1] (Descontrol)
    /*if(salida_SPI < 165) {
       juego.arkanoPi.raqueta.x= -2;		
      }
      else if (salida_SPI >= 165 && salida_SPI < 330 ) {
       juego.arkanoPi.raqueta.x= -1;		
      }
      else if (salida_SPI >= 330 && salida_SPI < 495 ) {
       juego.arkanoPi.raqueta.x= 0;		
      }	
      else if (salida_SPI >= 495 && salida_SPI < 660 ) {
       juego.arkanoPi.raqueta.x= 1;		
      }	
      else if (salida_SPI >= 660 && salida_SPI < 825 ) {
       juego.arkanoPi.raqueta.x= 2;		
      }	
      else if (salida_SPI >= 825 && salida_SPI < 990 ) {
	juego.arkanoPi.raqueta.x= 3;		
      }	
      else if (salida_SPI >= 990 && salida_SPI< 1155 ) {
       juego.arkanoPi.raqueta.x= 4;		
      }	
      else if (salida_SPI >= 1155 && salida_SPI < 1320 ) {
       juego.arkanoPi.raqueta.x= 5;		
      }
      else if (salida_SPI >= 1320 && salida_SPI < 1485 ) {
       juego.arkanoPi.raqueta.x= 6;		
      }	
      else if (salida_SPI >= 1485 && salida_SPI < 1650 ) {
       juego.arkanoPi.raqueta.x= 7;		
      }	
      else if (salida_SPI >= 1650 && salida_SPI < 1815 ) {
       juego.arkanoPi.raqueta.x= 8;		
      }	
      else if (salida_SPI >= 1815) {
	juego.arkanoPi.raqueta.x= 9;		
      }*/
	
      //OPCIóN 4: REGLA DE 3 CON SALIDA_SPI (Sólo adquiere valores pequeños, de -2 a 3)
      //juego.arkanoPi.raqueta.x= (salida_SPI * 11 /1984) - 2;

	
      //OPCIÓN 5: REGLA DE 3 CON ByteSPI[2] (Sólo adquiere valores pequeños, de -2 a 3)
      //juego.arkanoPi.raqueta.x= ((ByteSPI[2] >> 3) * 11 /63) - 2;
	
        #ifdef __MODO_DEBUG_TERMINAL__
        piLock (STD_IO_BUFFER_KEY);
        printf("\n\n Posicion! %d\n\n",juego.arkanoPi.raqueta.x );
        piUnlock (STD_IO_BUFFER_KEY);
        #endif
}

// void FinalJuego (void): funcion encargada de mostrar en la ventana de
// terminal los mensajes necesarios para informar acerca del resultado del juego.
void FinalJuego (fsm_t* this) {
       // A completar por el alumno... OKK

        piLock (FLAGS_KEY);
        flags &= ~FLAG_FINAL_JUEGO;
        piUnlock (FLAGS_KEY);
	
        PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &fin);
        delay(1500);

        piLock (STD_IO_BUFFER_KEY);
	
        printf("\n\n Ha finalizado el juego y ha perdido todas las vidas!!!\n\n");	

        //Calculamos el numero de ladrillos restantes.
        int nLadrillos = CalculaLadrillosRestantes(&(juego.arkanoPi.ladrillos));

       //Escribimos por pantalla el resultado del juego.
       if(nLadrillos ==0){
        //Actualmente, tal y como est¨¢ definido el juego no va a suceder, no puedes eliminar todos los ladrillos, pero...
        printf("\n\n Has ganado! \n\n");
       }
       else{printf("\n\n Has perdido el juego y has consumido todas las vidas, te han quedado %d ladrillos por destruir, intentalo de nuevo :)\n\n", nLadrillos );}

       piUnlock (STD_IO_BUFFER_KEY);

       //Activamos FLAGS_KEY para que la máquina de estados interprete que tenemos una tecla pulsada y directamente pase a WAIT_START y lance ReseteaJuego.
       //Si queremos  que sólo se produzca la transición al pulsar una tecla, deberíamos borrar estas líneas, pero...
       piLock (FLAGS_KEY);
       flags|= FLAG_TECLA;
       piUnlock (FLAGS_KEY);
	
       //Paramos los temporizadors de la pelota, de sonido y del joystick pues el juego ha acabado y ya no son necesarios.
       tmr_stop(juego.TIMER2);
       tmr_stop(juego.TIMER_JOYSTICK);
       tmr_stop(juego.TIMER_SONIDO);
}

//void ReseteaJuego (void): función encargada de llevar a cabo la
// reinicialización de cuantas variables o estructuras resulten
// necesarias para dar comienzo a una nueva partida.
void ReseteaJuego (fsm_t* this) {
       // A completar por el alumno... OKK

       //Pintamos un mensaje de bienvenida, un Hi!
       PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &bienvenida);

       //Desactivamos FLAGS_KEY pues ya hemos atendido el evento de tecla pulsada para resetear el juego
       piLock (FLAGS_KEY);
       flags &= ~FLAG_TECLA;
       piUnlock (FLAGS_KEY);
	
       //Escribimos unas instrucciones por pantalla.
       piLock (STD_IO_BUFFER_KEY);
       printf("\n");
       printf("***************************\n");
       printf("**********ARKANOPI*********\n");
       printf("***************************\n");

       PintaPantallaPorTerminal ((tipo_pantalla *)(& (juego.arkanoPi.pantalla)));
       printf("\nPULSE PARA COMENZAR EL JUEGO...\n");
       printf("i-Mueve la  raqueta a la  izquierda\n");
       printf("o-Mueve la  raqueta a la  derecha\n");
       printf("p-Mueve la pelota\n");
       printf("s-Para el juego\n");
       printf("s-Reanudar el juego\n");
       piUnlock (STD_IO_BUFFER_KEY);
}

//------------------------------------------------------
// FUNCIONES AUXILIARES
//------------------------------------------------------

// void ActualizaPosicionPelota (void): para actualizar la posicion de la pelota
//según la trayectoria.
void ActualizaPosicionPelota (void) {

       //Hacia abajo y derecha (diagonal)
       if((juego.arkanoPi.pelota.xv == 1 ) && (juego.arkanoPi.pelota.yv==1)){
        juego.arkanoPi.pelota.x+=1;
        juego.arkanoPi.pelota.y+=1;
       }
       //Hacia abajo (recto)
       else if((juego.arkanoPi.pelota.xv == 0 ) && (juego.arkanoPi.pelota.yv==1)){
        juego.arkanoPi.pelota.y+=1;
       } 
       //Hacia abajo e izquierda (diagonal)
       else if((juego.arkanoPi.pelota.xv == -1 ) && (juego.arkanoPi.pelota.yv==1)){
        juego.arkanoPi.pelota.x-=1;
        juego.arkanoPi.pelota.y+=1;
       }
       //Hacia arriba y izquierda (diagonal)
       else if((juego.arkanoPi.pelota.xv == -1 ) && (juego.arkanoPi.pelota.yv==-1)){
        juego.arkanoPi.pelota.x-=1;
        juego.arkanoPi.pelota.y-=1;
       }
       //Hacia arriba (recto)
       else if((juego.arkanoPi.pelota.xv ==0 ) && (juego.arkanoPi.pelota.yv==-1)){
        juego.arkanoPi.pelota.y-=1;
       }
       //Hacia arriba y derecha (diagonal)
       else if((juego.arkanoPi.pelota.xv == 1 ) && (juego.arkanoPi.pelota.yv==-1)){
        juego.arkanoPi.pelota.x+=1;
        juego.arkanoPi.pelota.y-=1;
       }
}

//void ReboteRaqueta (void)
void ReboteRaqueta (void) {

       //Hacemos sonar un sonido de una cierta frecuencia/ciclo de trabajo mediante PMW y en cuanto pase el tiempo indicado por CLK_SONIDO
       //nos vamos a la función desactivaSonido que se encarga de parar el sonido pues contiene la sentencia softToneWrite(pinSonido, 0);
       //que lo que hace es poner la frecuencia a 0 y por tanto parar el sonido como hemos visto en la documentación de la función.
       //[This updates the tone frequency value on the given pin. The tone will be played until you set the frequency to 0.]
       softToneWrite(pinSonido, 500);
       tmr_startms(juego.TIMER_SONIDO,CLK_SONIDO);

       #ifdef __MODO_DEBUG_TERMINAL__ 
       piLock (STD_IO_BUFFER_KEY);
       printf("\n\n Vas a rebotar con raqueta! \n\n");
       piUnlock (STD_IO_BUFFER_KEY);
       #endif

       //La pelota llega en diagonal hacia la derecha y descendente
       if(juego.arkanoPi.pelota.xv==1 && juego.arkanoPi.pelota.yv==1){

	 if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x){
	  juego.arkanoPi.pelota.xv=0;
	  juego.arkanoPi.pelota.yv=-1;
	 }

	 else if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x+1){
	  juego.arkanoPi.pelota.xv=1;
	  juego.arkanoPi.pelota.yv=-1;
	 }

	 else if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x+2){
	  juego.arkanoPi.pelota.xv=0;
	  juego.arkanoPi.pelota.yv=-1;
	 }
	}

	//La pelota llega recto descendente
        else if(juego.arkanoPi.pelota.xv==0 && juego.arkanoPi.pelota.yv==1){

	 if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x){
	  juego.arkanoPi.pelota.xv=-1;
	  juego.arkanoPi.pelota.yv=-1;
	 }

	 else if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x+1){
	  juego.arkanoPi.pelota.xv=0;
	  juego.arkanoPi.pelota.yv=-1;
	 }

	 else if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x+2){
	  juego.arkanoPi.pelota.xv=1;
	  juego.arkanoPi.pelota.yv=-1;
	 }
        }

	//La pelota llega en diagonal hacia la izquierda y descendente
	else if(juego.arkanoPi.pelota.xv==-1 && juego.arkanoPi.pelota.yv==1){

	 if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x){
	  juego.arkanoPi.pelota.xv=0;
	  juego.arkanoPi.pelota.yv=-1;
	 }

	 else if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x+1){
	  juego.arkanoPi.pelota.xv=-1;
	  juego.arkanoPi.pelota.yv=-1;
	 }

	 else if(juego.arkanoPi.pelota.x==juego.arkanoPi.raqueta.x+2){
	  juego.arkanoPi.pelota.xv=0;
	  juego.arkanoPi.pelota.yv=-1;
	 }
        }

       //***********************************************************//
       //MEJORA DEL PARPADEO tras rebote con raqueta//
       juego.arkanoPi.pantalla.matriz[juego.arkanoPi.pelota.x][juego.arkanoPi.pelota.y] = 0;
       juego.TIMER_ENCIENDE_PELOTA= tmr_new (enciendePelota);
       juego.TIMER_APAGA_PELOTA= tmr_new (apagaPelota);
       tmr_startms(juego.TIMER_ENCIENDE_PELOTA, CLK_PARPADEO_PELOTA);
       //***********************************************************//
}

//void RebotePared (void)
void RebotePared (void) {
	
      //Ponemos diferentes frecuencias dependiendo del objeto con el que choca para que el tono sea distinto!
      softToneWrite(pinSonido, 300);
      tmr_startms(juego.TIMER_SONIDO,CLK_SONIDO);

      #ifdef __MODO_DEBUG_TERMINAL__ 
      piLock (STD_IO_BUFFER_KEY);
      printf("\n\n Vas a rebotar con pared! \n\n");
      piUnlock (STD_IO_BUFFER_KEY);
      #endif
	
     juego.arkanoPi.pelota.xv=-(juego.arkanoPi.pelota.xv);
}

// void ReboteLadrillos (void)
void ReboteLadrillos (void) {	

      #ifdef __MODO_DEBUG_TERMINAL__ 
      piLock (STD_IO_BUFFER_KEY);
      printf("\n\n Vas a rebotar con ladrillos ! \n\n");
      piUnlock (STD_IO_BUFFER_KEY);
      #endif
	
      //Hacia arriba y izquierda (diagonal)
      if((juego.arkanoPi.pelota.xv == -1 ) && (juego.arkanoPi.pelota.yv==-1)){
       if(juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x-1][juego.arkanoPi.pelota.y-1]==1){
	juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x-1][juego.arkanoPi.pelota.y-1]=0;
	juego.arkanoPi.pelota.yv=-(juego.arkanoPi.pelota.yv);
			
	//***********************************************************//
        //MEJORA CAMBIO VELOCIDAD//
	cambiarVelocidad();
	//***********************************************************//


	//Tenemos que poner el sonido dentro de estos if's xq sólo en este caso nos aseguramos de que efectivamente ha chocado
	//con ladrillo, podría ocurrir que no había ladrillo pues había sido eliminado con anterioridad.
	softToneWrite(pinSonido, 700);
	tmr_startms(juego.TIMER_SONIDO,CLK_SONIDO);
	}
      }

      //Hacia arriba (recto)
      else if((juego.arkanoPi.pelota.xv ==0 ) && (juego.arkanoPi.pelota.yv==-1)){
       if(juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x][juego.arkanoPi.pelota.y-1]==1){
	juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x][juego.arkanoPi.pelota.y-1]=0;
	juego.arkanoPi.pelota.yv=-(juego.arkanoPi.pelota.yv);
			
	//***********************************************************//
        //MEJORA CAMBIO VELOCIDAD//
	cambiarVelocidad();
	//***********************************************************//

	softToneWrite(pinSonido, 700);
	tmr_startms(juego.TIMER_SONIDO,CLK_SONIDO);
	}
       }

       //Hacia arriba y derecha (diagonal)
      else if((juego.arkanoPi.pelota.xv == 1 ) && (juego.arkanoPi.pelota.yv==-1)){
	if(juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x+1][juego.arkanoPi.pelota.y-1]==1){
	 juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x+1][juego.arkanoPi.pelota.y-1]=0;
	 juego.arkanoPi.pelota.yv=-(juego.arkanoPi.pelota.yv);
			
	 //***********************************************************//
         //MEJORA CAMBIO VELOCIDAD//
	 cambiarVelocidad();
	 //***********************************************************//

	 softToneWrite(pinSonido, 700);
	 tmr_startms(juego.TIMER_SONIDO,CLK_SONIDO);
	 }
      }

      //Calculamos el número de ladrillos restantes por si sucede que los hemos eliminado todos tras el último movimiento
      //y debemos activar FLAG_FINAL_JUEGO. Tal y como está definido el juego actualmente no va a suceder pues no se pueden eliminar
      //todos los ladrillos, pero...
      if(CalculaLadrillosRestantes(&(juego.arkanoPi.ladrillos))==0){
       piLock (FLAGS_KEY);
       flags|= FLAG_FINAL_JUEGO;
       piUnlock (FLAGS_KEY);
      }
}

// void ReboteTecho (void)
void ReboteTecho (void) {

      softToneWrite(pinSonido, 300);
      tmr_startms(juego.TIMER_SONIDO,CLK_SONIDO);

      juego.arkanoPi.pelota.yv=-(juego.arkanoPi.pelota.yv);

      //Hacia abajo y izquierda (diagonal)
      if((juego.arkanoPi.pelota.xv == -1 ) && (juego.arkanoPi.pelota.yv==1)){
       if(juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x-1][juego.arkanoPi.pelota.y+1]==1){
	juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x-1][juego.arkanoPi.pelota.y+1]=0;
	juego.arkanoPi.pelota.xv=-(juego.arkanoPi.pelota.xv);				
	}
       }
       //Hacia abajo y derecha (diagonal)
       else if((juego.arkanoPi.pelota.xv == 1 ) && (juego.arkanoPi.pelota.yv==1)){
	if(juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x+1][juego.arkanoPi.pelota.y+1]==1){				
	 juego.arkanoPi.ladrillos.matriz[juego.arkanoPi.pelota.x+1][juego.arkanoPi.pelota.y+1]=0;
	 juego.arkanoPi.pelota.xv=-(juego.arkanoPi.pelota.xv);				
	}
       }
}

//------------------------------------------------------
// FUNCIONES DE MEJORA. ESTADO PARADA
//------------------------------------------------------

//MEJORA: Estado Parada
// void PararJuego (void): función que para el juego y muestra
// en la pantalla y en la matriz de leds un STOP
void PararJuego (fsm_t* this) {

	piLock (FLAGS_KEY);
	flags &= ~FLAG_STOP;
        flags &= ~FLAG_TECLA;
	piUnlock (FLAGS_KEY);

	PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &stop);
	
	#ifdef __MODO_DEBUG_TERMINAL__ 
	piLock (STD_IO_BUFFER_KEY);
        PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
	piUnlock (STD_IO_BUFFER_KEY);
	#endif
}


//MEJORA: Estado Parada
// void ReanudarJuego(void): función que reanuda el juego y muestra
// en la pantalla y en la matriz de leds todos los elementos (ladrillos, raqueta y pelota) de nuevo. Previamente muestra en la matriz de leds una flecha.
void ReanudarJuego (fsm_t* this) {

	piLock (FLAGS_KEY);
	flags &= ~FLAG_STOP;
	flags &= ~FLAG_TECLA;
	piUnlock (FLAGS_KEY);
	
	PintaMensajeInicialPantalla(&(juego.arkanoPi.pantalla), &play);
        delay (1500);

	ActualizaPantalla(&(juego.arkanoPi));
	
	#ifdef __MODO_DEBUG_TERMINAL__ 
	piLock (STD_IO_BUFFER_KEY);
	PintaPantallaPorTerminal((&(juego.arkanoPi.pantalla)));
        piUnlock (STD_IO_BUFFER_KEY);
	#endif
}

//------------------------------------------------------
// FUNCIONES DE TEMPORIZACION/REFRESCO MATRIZ LEDS
//------------------------------------------------------

//Se encarga de "hacer la tabla de verdad" del codificador pues hay que tener en cuenta que s¨®lo tenemos 4 pines para columnas
//y tenemos 10 columnas, por eso utilizamos un deco! 
void excitarColumna (void) {

	if(columna==0){
         digitalWrite (pinColumna0, 0);
         digitalWrite (pinColumna1, 0);
         digitalWrite (pinColumna2, 0);
         digitalWrite (pinColumna3, 0);
	}
	if(columna==1){
	 digitalWrite (pinColumna0, 1);
	 digitalWrite (pinColumna1, 0);
	 digitalWrite (pinColumna2, 0);
	 digitalWrite (pinColumna3, 0);
	}
	if(columna==2){
	 digitalWrite (pinColumna0, 0);
         digitalWrite (pinColumna1, 1);
	 digitalWrite (pinColumna2, 0);
         digitalWrite (pinColumna3, 0);
	}
	if(columna==3){
	 digitalWrite (pinColumna0, 1);
	 digitalWrite (pinColumna1, 1);
         digitalWrite (pinColumna2, 0);
	 digitalWrite (pinColumna3, 0);
	}
	if(columna==4){
         digitalWrite (pinColumna0, 0);
         digitalWrite (pinColumna1, 0);
	 digitalWrite (pinColumna2, 1);
         digitalWrite (pinColumna3, 0);
	}
	if(columna==5){
	 digitalWrite (pinColumna0, 1);
	 digitalWrite (pinColumna1, 0);
	 digitalWrite (pinColumna2, 1);
	 digitalWrite (pinColumna3, 0);
	}
	if(columna==6){
	 digitalWrite (pinColumna0, 0);
	 digitalWrite (pinColumna1, 1);
	 digitalWrite (pinColumna2, 1);
         digitalWrite (pinColumna3, 0);
	}
	if(columna==7){
	 digitalWrite (pinColumna0, 1);
	 digitalWrite (pinColumna1, 1);
	 digitalWrite (pinColumna2, 1);
	 digitalWrite (pinColumna3, 0);
	}
	if(columna==8){
         digitalWrite (pinColumna0, 0);
	 digitalWrite (pinColumna1, 0);
	 digitalWrite (pinColumna2, 0);
	 digitalWrite (pinColumna3, 1);
	}
	if(columna==9){
	 digitalWrite (pinColumna0, 1);
	 digitalWrite (pinColumna1, 0);
	 digitalWrite (pinColumna2, 0);
	 digitalWrite (pinColumna3, 1);
	}
}

//Se encarga del refresco de la matriz de leds.
void timer_refresco_display_isr(union sigval value) {

	int fila;
        
	//En primer lugar, elegimos la columna a iluminar.
	excitarColumna();

		
	for(fila=0;fila<7;fila++) {
             
	 //Fijamos la columna y vamos recorriendo las 7 filas.
	 if(juego.arkanoPi.pantalla.matriz[columna][fila]==1){
           //Hay que tener en cuenta que las filas, a diferencia de las columnas son activas a nivel bajo!
	   digitalWrite (matrizPinesFilas[fila], 0);
         }
	 else {
	  digitalWrite (matrizPinesFilas[fila], 1);
	 }
	}

	//Actualizar la columna a iluminar
	if(columna==9){
	 columna=0;
	}
	else {
	 columna++;
	}

        //Empieza la cuenta de nuevo.
	tmr_startms(juego.TIMER1, CLK_DISPLAY);
}

//------------------------------------------------------
// FUNCIONES DE INICIALIZACION
//------------------------------------------------------

// int systemSetup (void): procedimiento de configuracion del sistema.
// Realizar? entra otras, todas las operaciones necesarias para:
// configurar el uso de posibles librer¨ªas (e.g. Wiring Pi),
// configurar las interrupciones externas asociadas a los pines GPIO,
// configurar las interrupciones peri¨®dicas y sus correspondientes temporizadores,
// crear, si fuese necesario, los threads adicionales que pueda requerir el sistema
int system_setup (void) {
	int x = 0;

	piLock (STD_IO_BUFFER_KEY);

	// sets up the wiringPi library
	if (wiringPiSetupGpio () < 0) {
		printf ("Unable to setup wiringPi\n");
		piUnlock (STD_IO_BUFFER_KEY);
		return -1;
        }

	//Iniciamos el ADC
	if (wiringPiSPISetup (SPI_ADC_CH, SPI_ADC_FREQ) < 0) { //Conexion del canal 0 (GPIO 08 en numeracion BCM) a 1 MHz
	printf ("No se pudo inicializar el dispositivo SPI (CH 0)") ;
	exit (1);
	return -2;
	}

        #ifdef __MODO_DEBUG_TERMINAL__
	// Lanzamos thread para exploracion del teclado convencional del PC
	x = piThreadCreate (thread);
	if (x != 0) {
		printf ("it didn't start!!!\n");
		piUnlock (STD_IO_BUFFER_KEY);
		return -1;
        }
        #endif
	piUnlock (STD_IO_BUFFER_KEY);

	return 1;
}

void fsm_setup(fsm_t* arkanoPi_fsm) {
	piLock (FLAGS_KEY);
	flags = 0;
	piUnlock (FLAGS_KEY);

	ReseteaJuego(arkanoPi_fsm);
}

//------------------------------------------------------
// PI_THREAD (thread_explora_teclado): Thread function for keystrokes detection and interpretation
//------------------------------------------------------ 
PI_THREAD (thread) {
	int teclaPulsada;

	while(1) {
		delay(2000); // Wiring Pi function: pauses program execution for at least 10 ms

		piLock (STD_IO_BUFFER_KEY);

		if(kbhit()) {
			teclaPulsada = kbread();
			//printf("Tecla %c\n", teclaPulsada);

			switch(teclaPulsada) {
			 case 'i':
			 piLock (FLAGS_KEY);
			 flags |= FLAG_RAQUETA_IZQUIERDA;
			 flags |= FLAG_TECLA;
			 piUnlock (FLAGS_KEY);
			 break;

			case 'o':
			 piLock (FLAGS_KEY);
			 flags |= FLAG_RAQUETA_DERECHA;
			 flags |= FLAG_TECLA;
			 piUnlock (FLAGS_KEY);
			break;

			case 'p':
			 piLock (FLAGS_KEY);
			 flags |= FLAG_PELOTA;
			 flags |= FLAG_TECLA;
			 piUnlock (FLAGS_KEY);
			break;

			case 's':
		         piLock (FLAGS_KEY);
			 flags |= FLAG_STOP;
			 flags |= FLAG_TECLA;
			 piUnlock (FLAGS_KEY);
		        break;

			default:
			 printf("INVALID KEY!!!\n");
			break;
			}
		}

		piUnlock (STD_IO_BUFFER_KEY);
	}
}

int main () {
	unsigned int next;

	//Creamos el temporizador del refresco de la matriz de leds
        juego.TIMER1= tmr_new (timer_refresco_display_isr);
        tmr_startms(juego.TIMER1, CLK_DISPLAY);

	wiringPiSetupGpio();
	//Ponemos los pines de las columnas y filas a OUTPUT
	pinMode (pinColumna0, OUTPUT);
	pinMode (pinColumna1, OUTPUT);
	pinMode (pinColumna2, OUTPUT);
	pinMode (pinColumna3, OUTPUT);
	pinMode (pinFila0, OUTPUT);
	pinMode (pinFila1, OUTPUT);
	pinMode (pinFila2, OUTPUT);
	pinMode (pinFila3, OUTPUT);
	pinMode (pinFila4, OUTPUT);
	pinMode (pinFila5, OUTPUT);
	pinMode (pinFila6, OUTPUT);

	//Ponemos los pines de los pulsadores a INPUT
	pinMode (pinPulsadorIzquierdo, INPUT);
	pinMode (pinPulsadorDerecho, INPUT);		
	pinMode (pinPulsadorStop, INPUT);
		
	//Interrupciones externas. Si detecta un flanco de bajada en los pines de los pulsadores, lanza la funci¨®n indicada
	//para activar los flags correspondientes. (Los pulsadores son activos a nivel bajo!)
	wiringPiISR (pinPulsadorIzquierdo, INT_EDGE_FALLING, activaFlagPulsadorIzquierdo);
	wiringPiISR (pinPulsadorDerecho, INT_EDGE_FALLING, activaFlagPulsadorDerecho);
	wiringPiISR (pinPulsadorStop, INT_EDGE_FALLING, activaFlagPulsadorStop);
				
	//Ponemos el pin  de sonido a OUTPUT
	pinMode (pinSonido, OUTPUT);

        //Indicamos que pin vamos a utlizar para el sonido con PWM
	softToneCreate(pinSonido);


	// Maquina de estados: lista de transiciones
	// {EstadoOrigen,Funci¨®nDeEntrada,EstadoDestino,Funci¨®nDeSalida}
	fsm_trans_t tabla[] = {
	 { WAIT_PUSH, compruebaJoystick, WAIT_PUSH,  ControlJoystick  },
	 { WAIT_START, compruebaTeclaPulsada,  WAIT_PUSH, InicializaJuego },
	 { WAIT_PUSH, compruebaTeclaPelota, WAIT_PUSH,  MovimientoPelota },
	 { WAIT_PUSH, compruebaTeclaRaquetaDerecha, WAIT_PUSH,  MueveRaquetaDerecha },
	 { WAIT_PUSH, compruebaTeclaRaquetaIzquierda, WAIT_PUSH,  MueveRaquetaIzquierda },
	 { WAIT_PUSH, compruebaFinalJuego, WAIT_END, FinalJuego },
	 { WAIT_END, compruebaTeclaPulsada, WAIT_START, ReseteaJuego },
	 { WAIT_PUSH, compruebaTeclaStop, WAIT_STOP, PararJuego },
	 { WAIT_STOP, compruebaTeclaStop, WAIT_PUSH,  ReanudarJuego },
         { -1, NULL, -1, NULL },
	};

	fsm_t* arkanoPi_fsm = fsm_new (WAIT_START, tabla, NULL);

	// Configuracion e inicializaci¨®n del sistema
	system_setup();
	fsm_setup (arkanoPi_fsm);	

	next = millis();
	while (1) {
	 fsm_fire (arkanoPi_fsm);
	 next += CLK_MS;
	 delay_until (next);
	}

	fsm_destroy (arkanoPi_fsm);
	tmr_destroy (juego.TIMER1);
	tmr_destroy (juego.TIMER2);
	tmr_destroy (juego.TIMER_SONIDO);
	tmr_destroy (juego.TIMER_JOYSTICK);

}
