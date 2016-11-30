#ifndef ENEMIES_H
#define ENEMIES_H

#include "structures.h"

/** Az ellenségek funkcióinak leírása az enemies.c-ben található **/
void AddEnemy(EnemyListStart*, Vec2, Uint8, Sint8);
void RemoveEnemy(EnemyListStart*, EnemyList*);
void EmptyEnemyList(EnemyListStart*);
Uint8 Intersect(Vec2, Vec2, Vec2, Vec2);
void EnemyListTick(EnemyListStart*, PlayerObject*, Uint8*, ShotList*, Uint8, Uint8*);
FILE* GetLevel(Uint8);
void LevelSpawner(EnemyListStart*, Uint8);
Enemy GetEnemy(Uint8);
void FreeDynamicEnemies();

#endif /* ENEMIES_H */
