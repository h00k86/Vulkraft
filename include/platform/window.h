
//
// helper per inizializzare/distruggere la finestra
//
//

#pragma once


/* PORTING (VALUTAZIONI.md E14, 2026-09-25): qui NON si definisce piu'
 * GLFW_INCLUDE_VULKAN. Lo faceva, e siccome app.h include questo header,
 * <vulkan/vulkan.h> finiva dentro tutti i 104 file che includono app.h --
 * anche quelli che di GLFW volevano solo GLFWwindow. Le due sole funzioni
 * di interoperabilita' GLFW/Vulkan (glfwCreateWindowSurface e
 * glfwGetRequiredInstanceExtensions) sono usate da vulkan_surface.c e
 * vulkan_instance.c, che definiscono la macro per conto loro prima di
 * includere questo header. */
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
typedef struct APP APP;





GLFWwindow * create_window( APP *app);
void destroy_window(GLFWwindow *window);

// Monitor che contiene la porzione maggiore della finestra. Per una finestra
// fullscreen GLFW fornisce direttamente il monitor associato; senza
// intersezione valida il primary resta solo un fallback.
GLFWmonitor *glfw_helper_get_window_monitor(GLFWwindow *window);
int glfw_helper_get_monitor_index(GLFWmonitor *monitor);


/* P3-B: the cached window size, refreshed by GLFW's window-size callback.
 * Replaces a per-frame glfwGetWindowSize (an X11 round trip) on the hot path. */
void glfw_helper_window_size(int *outWidth, int *outHeight);
