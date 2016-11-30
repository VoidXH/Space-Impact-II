#ifndef SCENERY_H
#define SCENERY_H

#include "structures.h"

/** A táj funkcióinak leírása a scenery.c-ben található **/
void AddScenery(SceneryList*, Graphics, Vec2);
void EmptyScenery(SceneryList*);
void HandleScenery(SceneryList*, Uint8*, Uint8, PlayerObject*, Sint8);

#endif /* SCENERY_H */
