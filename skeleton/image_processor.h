#ifndef IMAGE_PROCESSOR_H /* previne includerea multipla a headerului */
#define IMAGE_PROCESSOR_H

#include "queue.h"  /* pentru tipul ImageTask */

/*
 * proceseaza o sarcina din coada
 * accepta fisiere JPEG si PNG
 *  - JPEG -> recomprimare cu quality 75
 *  - PNG  -> rescriere cu libpng si compresie maxima
 * rezultatul este salvat pe disk
 */
int process_image_task(const ImageTask *task);

#endif