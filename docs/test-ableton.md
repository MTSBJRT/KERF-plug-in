# Protocole de test ChopForge dans Ableton Live (macOS)

Objectif : vérifier en 15 minutes ce qui n'a pas pu être testé en dehors d'un DAW :
chargement, synchronisation au transport, capture calée sur la mesure, MIDI, et
sauvegarde dans le projet. Notez le résultat de chaque étape (OK / KO + ce qui s'est passé).

## 0. Installation

```bash
cd ChopForge
./scripts/build-macos.sh
```

Attendu en fin de script : `All engine tests passed.`, `All host tests passed.` et une
ligne auval se terminant par `AU VALIDATION SUCCEEDED`.

Dans Live : Préférences > Plug-Ins. Activez *Use VST3 Plug-In System Folders* (le dossier
utilisateur `~/Library/Audio/Plug-Ins/VST3` est inclus) et, si vous voulez tester l'AU,
*Use Audio Units*. Cliquez *Rescan* en maintenant Alt (Option) enfoncé pour forcer un
scan complet. ChopForge doit apparaître dans le navigateur, catégorie Plug-Ins > VST3 >
Bajart Audio (et Audio Units > Bajart Audio).

## 1. Chargement et interface

1. Créez une piste audio, glissez ChopForge dessus. Ouvrez la fenêtre du plugin.
2. Attendu : interface complète, en-tête « 120.0 BPM STOP | 0 voices » (le tempo doit
   refléter celui de Live, pas 120 si votre set est à un autre tempo).
3. Redimensionnez la fenêtre : tout doit suivre sans chevauchement.

## 2. Chargement d'un fichier et grille

1. Glissez une boucle de batterie (wav/aif, 1 à 2 mesures) sur la fenêtre du plugin, ou
   cliquez LOAD.
2. Attendu : forme d'onde affichée, 16 slices numérotées, nom du fichier et durée dans
   l'en-tête.
3. Passez *Slicing* sur *Transients* : les marqueurs doivent se placer sur les attaques.
   Baissez/montez *Sens* : moins/plus de marqueurs.
4. Double-cliquez dans la forme d'onde : un marqueur s'ajoute et *Slicing* passe sur
   *Manual*. Glissez-le, puis clic droit > *Remove this marker*.

## 3. Synchronisation au transport (le point le plus important)

1. Mettez un clip audio quelconque sur la piste (ou rien : le plugin joue seul).
   Lancez la lecture de Live.
2. Attendu : la colonne active de la grille avance en rythme, l'en-tête passe à
   « PLAY », les slices colorées s'allument dans la forme d'onde, et on entend la
   boucle rejouée à la double-croche.
3. Changez le tempo de Live pendant la lecture : le plugin doit suivre immédiatement.
4. Arrêtez : le son s'arrête, la colonne active disparaît. Relancez depuis le début :
   le pas 1 doit tomber sur le premier temps.
5. Mettez *Step* sur 1/8T puis 1/32 : la vitesse change sans décalage cumulé.
6. Activez une boucle de 1 mesure dans Live : à chaque retour au début de la boucle, le
   motif doit repartir au pas 1 sans double déclenchement ni trou.
7. Montez *Swing* à 50 % : les pas pairs sont retardés.

## 4. Capture live

1. Placez ChopForge sur une piste de batterie qui joue (ou sur le Master).
2. *Capture* = 2 bars. Lancez la lecture, puis cliquez CAPTURE.
3. Attendu : le bouton affiche ARMED, puis REC xx % à partir du **début de la mesure
   suivante**, puis la forme d'onde capturée s'affiche et les slices rejouent le contenu.
4. Vérifiez que le début de la capture tombe bien sur le premier temps (le premier slice
   doit contenir le kick du temps 1, pas la fin de la mesure précédente).

## 5. MIDI

1. Créez une piste MIDI. Dans *MIDI To*, choisissez la piste audio qui porte ChopForge,
   puis dans le second menu choisissez *ChopForge* (et non *Track In*).
2. Armez la piste MIDI. Jouez C1, C#1, D1… au clavier (ou au pavé de notes de Live).
3. Attendu avec *MIDI* = One-shot : chaque note joue le slice correspondant en entier.
   Avec *Hold* : le slice s'arrête quand on relâche la touche. Avec *Chromatic* : les
   notes autour de C3 transposent le slice choisi par *Chrom*.
4. Mettez *Clock* sur *Off* : le séquenceur se tait, seul le MIDI joue.

## 6. Randomisation, patterns, undo

1. Cliquez RANDOMIZE plusieurs fois, puis UNDO / REDO : le motif revient exactement.
2. Verrouillez quelques cellules S/P/F, randomisez : les cellules verrouillées ne bougent
   pas.
3. Choisissez *Scale* = Minor, cochez *Pitch*, randomisez : les pitchs affichés sont
   bien dans la gamme.
4. Copiez le pattern A, passez en B (vide), collez. Automatisez le paramètre
   « Pattern » dans Live : le changement de slot doit suivre l'automation.
5. Sur un pas : *Repeat* x4 et *Gate* 50 % : on entend un roulement.
   *Filter* vers la gauche : passe-bas, vers la droite : passe-haut.
   *Prob* 50 % : le pas ne joue qu'une fois sur deux.

## 7. Sauvegarde du projet

1. Enregistrez le set Live, fermez-le, rouvrez-le.
2. Attendu : la forme d'onde, les marqueurs, les 8 patterns et tous les réglages sont
   revenus. (L'audio est stocké dans le set, compressé en FLAC.)
3. Dupliquez la piste : la copie doit contenir le même audio et le même pattern.

## 8. Charge CPU

Avec 4 instances en lecture, regardez le vumètre CPU de Live. Notez la valeur et la
taille de buffer audio (Préférences > Audio).

## Que renvoyer en cas de problème

- La sortie complète de `./scripts/build-macos.sh`.
- Pour un plantage : le rapport dans Console.app > Rapports de plantage (fichier
  `Live_*.ips` ou `ChopForge_*.ips`), ou le contenu de `~/Library/Logs/DiagnosticReports`.
- Pour un problème de scan : Live > Préférences > Plug-Ins, le message affiché, et le
  résultat de la commande

```bash
auval -v aumf Chpf Bjrt
```

- Pour un problème de synchronisation : le tempo, la signature rythmique, la position de
  la boucle, et ce qui est observé (décalage constant ? dérive ? double déclenchement ?).
