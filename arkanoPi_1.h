#ifndef _ARKANOPI_H_
#define _ARKANOPI_H_

//***********************************************************//
//MEJORA DEL PARPADEO tras rebote con raqueta//
#define CLK_PARPADEO_PELOTA 100
//***********************************************************//

//LIBRERÍAS
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h> // para poder usar NULL
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <wiringPi.h>

#include "fsm.h" // para poder crear y ejecutar la máquina de estados
#include "tmr.h" // para poder usar temporizadores

#include "kbhit.h" // para poder detectar teclas pulsadas sin bloqueo y leer las teclas pulsadas

#include "arkanoPiLib.h"

#include <softTone.h> // para efectos de sonido mediante PWM

//Pines Columnas y filas
#define pinColumna0 14
#define pinColumna1 17
#define pinColumna2 18
#define pinColumna3 22
#define pinFila0 0
#define pinFila1 1
#define pinFila2 2
#define pinFila3 3
#define pinFila4 4
#define pinFila5 7
#define pinFila6 23

//PINES PULSADORES
#define pinPulsadorIzquierdo 16
#define pinPulsadorDerecho 19
#define pinPulsadorStop 20

//Pin Sonido
#define pinSonido 25

//Timeout temporizadores
#define CLK_MS 10 // PERIODO DE ACTUALIZACION DE LA MAQUINA ESTADOS
#define CLK_DISPLAY 1
//#define CLK_PELOTA 750 //SUSTITUIR POR velocidad SI QUEREMOS QUE LA VELOCIDAD DE LA PELOTA NO SEA ELEGIDA POR EL USUARIO
#define CLK_SONIDO 200
#define CLK_JOYSTICK 50

// Debounce time in milisecs
#define DEBOUNCE_TIME 200

//MEJORA: Joystick
#include <wiringPiSPI.h>
#define SPI_ADC_CH 0
#define SPI_ADC_FREQ 1000000

// FLAGS DEL SISTEMA
#define FLAG_TECLA 0x01
#define FLAG_PELOTA 0x02
#define FLAG_RAQUETA_DERECHA 0x04
#define FLAG_RAQUETA_IZQUIERDA 0x08
#define FLAG_FINAL_JUEGO 0x10
#define FLAG_STOP 0x20
#define FLAG_JOYSTICK 0x40

typedef enum {
	WAIT_START,
	WAIT_PUSH,
	WAIT_STOP,
	WAIT_END} tipo_estados_juego;

typedef struct {
	tipo_arkanoPi arkanoPi;
	tipo_estados_juego estado;
	char teclaPulsada;
	tmr_t* TIMER1; //Timer de refresco de la matriz  de leds
	tmr_t* TIMER2; //Timer de la pelota
	tmr_t* TIMER_SONIDO; //Timer de sonido
	tmr_t* TIMER_JOYSTICK; //Timer del joystick
	tmr_t* TIMER_APAGA_PELOTA; //Timer apagar led pelota
	tmr_t* TIMER_ENCIENDE_PELOTA; //Timer encender led pelota
} tipo_juego;

// A 'key' which we can lock and unlock - values are 0 through 3
//	This is interpreted internally as a pthread_mutex by wiringPi
//	which is hiding some of that to make life simple.
#define	FLAGS_KEY	1
#define	STD_IO_BUFFER_KEY	2

//------------------------------------------------------
// FUNCIONES MEJORAS CONVOCATORIA EXTRAORDINARIA
//------------------------------------------------------
void enciendePelota (union sigval value);
void apagaPelota(union sigval value);
void cambiarVelocidad(void);

//------------------------------------------------------
// FUNCIONES DE RETARDO
//------------------------------------------------------
void delay_until (unsigned int next);

//------------------------------------------------------
// FUNCIONES DE ACTIVACIÓN DE LOS PULSADORES
//------------------------------------------------------
void activaFlagPulsadorDerecho(void);
void activaFlagPulsadorIzquierdo(void);
void activaFlagPulsadorStop(void);

void activaFlagPelota(union sigval value);

void activaFlagJoystick(union sigval value);

//------------------------------------------------------
// FUNCIÓN DE DESACTIVACIÓN SONIDO
//------------------------------------------------------
void desactivaSonido(union sigval value);

//------------------------------------------------------
// FUNCIONES DE ENTRADA O DE TRANSICION DE LA MAQUINA DE ESTADOS
//------------------------------------------------------
// Prototipos de funciones de entrada
int compruebaTeclaPulsada (fsm_t* this);
int compruebaTeclaPelota (fsm_t* this);
int compruebaTeclaRaquetaDerecha (fsm_t* this);
int compruebaTeclaRaquetaIzquierda (fsm_t* this);
int compruebaTeclaStop (fsm_t* this);
int compruebaFinalJuego (fsm_t* this);
int compruebaJoystick (fsm_t* this);

//------------------------------------------------------
// FUNCIONES DE SALIDA O DE ACCION DE LA MAQUINA DE ESTADOS
//------------------------------------------------------
void InicializaJuego(fsm_t*);
void MueveRaquetaIzquierda(fsm_t*);
void MueveRaquetaDerecha (fsm_t*);
void MovimientoPelota(fsm_t*);
void FinalJuego(fsm_t*);
void ReseteaJuego(fsm_t*);
void ControlJoystick(fsm_t*);
void PararJuego (fsm_t* this);
void ReanudarJuego (fsm_t* this);

//------------------------------------------------------
// FUNCIONES AUXILIARES
//------------------------------------------------------
void ActualizaPosicionPelota (void);
void ReboteRaqueta (void);
void RebotePared (void);
void ReboteLadrillos (void);
void ReboteTecho (void);

//------------------------------------------------------
// FUNCIONES DE MEJORA. ESTADO PARADA
//------------------------------------------------------
void PararJuego (fsm_t* this);
void ReanudarJuego (fsm_t* this);

//------------------------------------------------------
// FUNCIONES DE TEMPORIZACION/REFRESCO MATRIZ LEDS
//------------------------------------------------------
void excitarColumna (void);
void timer_refresco_display_isr(union sigval value);

//------------------------------------------------------
// FUNCIONES DE INICIALIZACION
//------------------------------------------------------
int systemSetup (void);
void fsm_setup(fsm_t* arkanoPi_fsm);
//------------------------------------------------------
// SUBRUTINAS DE ATENCION A LAS INTERRUPCIONES
//------------------------------------------------------
PI_THREAD (thread);

#endif /* ARKANOPI_H_ */
