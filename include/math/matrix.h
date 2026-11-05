#pragma once

#ifndef MATRIX_H
#define MATRIX_H

typedef struct {
    float m[16];
} mat4;

// Funzione per ottenere una matrice identità (non fa nulla, utile per test)
mat4 mat4_identity();

// Funzione per la proiezione ortografica (Risoluzione fissa)
mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far);

// MACROTASK Z1 -- ortografica con profondita' REALE nella convenzione
// Vulkan. mat4_ortho sopra e' la formula OpenGL (NDC z in [-1,1]): in
// Vulkan il depth range e' [0,1] e ogni z NDC negativo viene CLIPPATO --
// funzionava solo perche' ogni vertice 2D stava esattamente a z=0 (mappato
// su 0, il bordo near). Questa variante mappa worldZ in [zBack, zFront]
// su depth [1,0]: zFront (WORLD_Z_MAX) -> 0.0 = davanti, zBack
// (WORLD_Z_MIN) -> 1.0 = dietro, quindi "worldZ piu' alto = piu' vicino"
// (stessa intuizione del vecchio zIndex). Con depth test LESS_OR_EQUAL e
// clear a 1.0, un frammento a depth minore vince. Nessuna dipendenza
// dalle altre funzioni (nessuna moltiplicazione 4x4 generica ancora).
mat4 mat4_ortho_world_z(float left, float right, float bottom, float top,
                        float zBack, float zFront);

// Funzione per la traslazione (Muovere la camera)
mat4 mat4_translate(mat4 m, float x, float y, float z);

// Scala centrata su (centerX, centerY) invece che sull'origine — zoom
// > 1.0 fa vedere PIÙ mondo (zoom out): le distanze dal centro vengono
// DIVISE per zoom, non moltiplicate (x' = centerX + (x-centerX)/zoom).
// Formula chiusa diretta, non una composizione di matrici: questa
// libreria non ha ancora una moltiplicazione 4x4 generica,
// mat4_translate funziona solo perché parte sempre da un'identità.
// zoom=1.0 restituisce l'identità esatta.
mat4 mat4_scale_around(float zoom, float centerX, float centerY);

#endif

/* ---- MACROTASK Z2: algebra 4x4 generica (colonna-major, come sopra) ----
 * m[col*4 + row]. Nessuna delle funzioni precedenti e' stata modificata:
 * mat4_translate/mat4_scale_around restano le formule chiuse di sempre. */
#ifndef MATRIX_H_Z2
#define MATRIX_H_Z2
mat4 mat4_multiply(mat4 a, mat4 b); /* a * b (applica prima b, poi a) */
mat4 mat4_translation(float x, float y, float z);
mat4 mat4_scaling(float x, float y, float z);
/* Rotazioni in GRADI, verso positivo = antiorario guardando dall'asse
 * positivo verso l'origine (regola della mano destra). */
mat4 mat4_rotation_x(float degrees);
mat4 mat4_rotation_y(float degrees);
mat4 mat4_rotation_z(float degrees);
/* Applica m a (x,y,z,1); w viene ignorato (nessuna divisione prospettica:
 * le matrici usate dal renderer sono affini/ortografiche). */
void mat4_transform_point(const mat4 *m, const float in[3], float out[3]);
#endif
