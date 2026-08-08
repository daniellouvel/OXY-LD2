#include <Arduino.h>

// Point d'entree firmware. La logique metier vit dans lib/ (voir ARCHITECTURE.md) -
// ce fichier orchestre les modules, il ne doit pas grossir en logique propre.

void setup() {
  Serial.begin(115200);
  Serial.println("OXY-LD2 - boot");
}

void loop() {
}
