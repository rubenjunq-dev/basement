# BASEMENT — vocal chain plugin

Plugin d'effet (VST3 / AU / Standalone) pour le mix de voix. Une chaîne complète, entièrement réglable, avec des presets "par artiste" qu'on parcourt avec les flèches ‹ › en haut.

```
INPUT → TUNE → EQ 4 bandes → COMP → DE-ESS → SATURATE → DOUBLE → DELAY → REVERB → OUTPUT
```

| Module | Réglages |
|---|---|
| INPUT | gain, high-pass 24 dB/oct, gate |
| TUNE | autotune (tonalité, gamme, vitesse 0 ms = robotique, force) + transposition ±12 demi-tons, affichage de la note détectée |
| EQ | low shelf / 2 bells / high shelf — gain, fréquence, Q chacune, courbe en direct |
| COMP | seuil, ratio, attaque, release, makeup, mix parallèle, compteur de réduction |
| DE-ESS | fréquence, seuil, force |
| SATURATE | Tape / Tube / Hard / Fold, drive, tone, mix |
| DOUBLE | rate, depth, mix (deux voix désaccordées, stéréo) |
| DELAY | libre ou synchro tempo (1/2 → 1/16, pointé, triolet), feedback, filtre, ping-pong, mix |
| REVERB | size, damp, width, pre-delay, low cut, mix |
| OUTPUT | gain, dry/wet global, safe clip |

Chaque module a sa LED on/off. Double-clic sur un knob = valeur par défaut. Molette = réglage fin.
La couleur de l'interface change avec l'artiste sélectionné.

---

## Compiler

### Option A — le plus simple : GitHub compile pour toi (Windows + Mac)

1. Crée un dépôt GitHub (privé si tu veux) et pousse ce dossier dedans.
2. Onglet **Actions** → le workflow "Build plugin" démarre tout seul.
3. Quand il est vert (10-20 min la première fois), ouvre le run → **Artifacts** → télécharge `Basement-Windows` et/ou `Basement-macOS`.

### Option B — en local

Prérequis : CMake ≥ 3.22 et un compilateur C++17 (Visual Studio 2022 sur Windows, Xcode sur Mac, gcc/clang + libs X11/ALSA sur Linux).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Basement_VST3
```

JUCE est téléchargé automatiquement au premier `cmake -B build`.
Le fichier est dans `build/Basement_artefacts/Release/VST3/Basement.vst3`.

### Installer

| OS | Dossier |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS (VST3) | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS (AU, pour Logic / GarageBand) | `~/Library/Audio/Plug-Ins/Components/` |

Puis rescanner les plugins dans la DAW. Pour tester sans DAW : lance la version **Standalone**.

> macOS : un plugin non signé est bloqué par Gatekeeper. Pour toi, clic droit → Ouvrir, ou
> `xattr -dr com.apple.quarantine Basement.vst3`. Pour distribuer à des clients, il faudra
> signer + notariser (compte Apple Developer). Sous Windows, un certificat de signature évite les alertes SmartScreen.

### Compatibilité DAW

VST3 + AU couvrent Ableton Live, FL Studio, Reaper, Cubase/Nuendo, Studio One, Bitwig, Logic, GarageBand, Ardour…
**Pro Tools** demande le format AAX : il faut le SDK AAX (compte développeur Avid) + signature PACE. Non inclus.

---

## Modifier les presets / ajouter un artiste

Tout est dans `Source/Presets.h`. Copie un bloc `{ "NOM", "tagline", 0xffRRGGBB, { ... } }`, change les valeurs, recompile.
Les paramètres non listés reviennent à leur valeur par défaut. Les presets sont aussi exposés à la DAW (liste de programmes).

Les chaînes fournies sont des **interprétations** de l'esthétique de chaque artiste, pas leurs vraies chaînes : à affiner à l'oreille.

## Structure du code

```
Source/
  Params.h            liste unique des paramètres (ID, plages, affichage)
  DSP.h               tous les effets (biquad, gate, YIN + pitch-shift, comp, de-esser, sat, delay, reverb…)
  Presets.h           presets d'usine
  PluginProcessor.*   chaîne de traitement, états, presets, latence
  PluginEditor.*      interface (grille 1120×584, redimensionnable)
  UI/                 look & feel, knobs, panneaux, courbe d'EQ, lecteur de note
Tests/TestMain.cpp    banc de test (mesures objectives) + export de captures de l'interface
```

Banc de test :

```bash
cmake -B build -DBASEMENT_BUILD_TESTS=ON && cmake --build build --target BasementTests
./build/BasementTests_artefacts/BasementTests                   # tests audio
./build/BasementTests_artefacts/BasementTests --snapshot out/   # captures PNG
```

## Limites connues (v0.1)

- **Latence fixe de 12,5 ms** (pitch-shifter). Elle est reportée à la DAW et compensée, donc OK pour le mix ; pour enregistrer en monitoring direct, passer par le monitoring de l'interface audio.
- **Autotune** : monophonique, algorithme temporel léger. Très bien pour l'effet "robotique / hyperpop", moins transparent qu'Auto-Tune Pro ou Melodyne sur une correction subtile.
- **Saturation** : pas de sur-échantillonnage (un peu d'aliasing aux gros drives, ça fait partie du grain).
- **Reverb** : Freeverb de JUCE, correcte mais pas "premium". Un FDN maison serait un vrai upgrade.
- Vérifié : compilation Linux (VST3 + banc de test), mesures audio automatiques (`Tests/`), validation **pluginval niveau 5** (chargement, états, automation, buffers 32→1024, 44.1→96 kHz, ouverture de l'éditeur). **Pas encore testé à l'écoute, ni compilé/chargé sur Windows/Mac dans une vraie DAW** : c'est la prochaine étape.

## Licences à vérifier avant de vendre

- **JUCE** est en double licence (GPLv3 / licence commerciale). Pour vendre un plugin fermé, il faut une licence JUCE commerciale adaptée à ton chiffre d'affaires. Vérifie les conditions actuelles sur juce.com. Le splash screen JUCE s'affiche tant qu'on n'a pas une licence qui permet de le retirer.
- **VST** est une marque de Steinberg : le SDK VST3 est inclus dans JUCE sous licence permissive, mais la mention "VST is a trademark of Steinberg Media Technologies GmbH" est exigée dans la communication.
- **Noms d'artistes** : voir l'avertissement en haut de `Presets.h`.
