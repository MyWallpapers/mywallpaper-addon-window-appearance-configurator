# Preuves source A06, 13 octobre 2026

Cette note est une preuve d’implémentation de la mission locale, subordonnée au ledger. Commit MyWallpaper examiné : `299548ebcc20770fd73b10bdffd5f05586fbe126`. Ledger SHA-256 : `ed477485a495b1e6ff9e931dfae5378461378c4c57977a4e436e753ca9ddba6c`, vérifié au début et à la fin de l’étude. DEC-0012/0013 : comportements propres à l’add-on et contrats génériques ; DEC-0099 : licence et identité cohérente de surface ; DEC-0157 : la mission ne ratifie pas une portée de lancement ; DEC-0160 : négociation alternative Windhawk fermée et qualification de frontière GPL conservée. Ces points sont conformes ; aucune décision n’est amendée.

Avant les lectures ciblées, Codebase Memory a été interrogée : architecture du configurateur taskbar, recherche du lifecycle/companion dans le moteur et recherche des schémas de réglages dans MyWallpaper. Les fichiers actuels ont ensuite été vérifiés directement.

## Référence taskbar

Révision `8e1fe2e6ddfa1acd51f9eb626f4f0dab09c21dc0`. README, manifeste, parseur `src/config.cpp`, `BuildDirectProperties`, callbacks `Start/SettingsChanged/Stop` et le SDK natif ont été lus. La référence comporte 45 définitions, six sections et 39 valeurs, avec profils, scope appareil et conditions d’affichage. Son rendu direct emploie des brosses unies et Windows Composition pour les états pointeur, et restaure les propriétés XAML modifiées. Ce dépôt reprend la direction des contrôles habituels ; son rendu de fenêtre est une implémentation différente, externe, avec dégradés réels calculés par son renderer. Il n’adopte pas les sélecteurs XAML ni les animations de barre pour les fenêtres.

## Mods A06 et frontière des mécanismes

Les entrées de `familles.json` et les mods A06 de `catalogue.json` ont été lus. Les liens ci-dessous sont épinglés sur `8d60b4c4113a7f0a18b4b48a5443a199acf54805`, pas sur une branche mouvante. Le rapport courant est un apport de preuve, sans validation runtime des mods concurrents.

| Source étudiée | Technique vérifiée | Conséquence pour cet add-on |
| --- | --- | --- |
| [Translucent Windows](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/translucent-windows.wh.cpp) | Différencie AccentBlur et SystemBackdrop ; son rendu de thèmes/GDI doit gérer l’alpha, et les surfaces Chromium/Qt/WinUI peuvent masquer l’effet. | Demander un matériau natif ne prouve pas le verre visible dans le contenu. Aucune modification globale des couleurs système n’est reprise. |
| [Custom Window Corner Radius](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/custom-corner-radius.wh.cpp) | Hooks de symboles `udwm.dll`, chemins de rayon différents selon le build et filtrage maximisé/snappé. | Le rayon par région Win32 livré est une découpe différente. Le vrai rayon DWM exige le résolveur privé proposé, plus une qualification de chaque ABI. |
| [Native Shadow Tuner](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/native-shadow-tuner.wh.cpp) | Symboles privés de paramètres d’ombre ; validation du code/offsets pour les variantes dynamiques. | Les ombres livrées sont des décorations additionnelles. Aucun réglage de l’ombre native n’est annoncé. |
| [Titlebar For Everyone](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/titlebar-for-everyone.wh.cpp) | Subclassing dans les vrais processus et traitement distinct des classes Chromium, WPF, WinUI, Office, Qt et Steam ; traitement de créations/parents et de contrôles spécifiques. | Changer `WS_CAPTION` ne suffit pas à prouver la conversion de toutes ces applications. Le titre compagnon livré conserve le contenu ; l’adaptation in-process demeure un besoin séparé. |
| [Borderless Fullscreen](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/borderless-fullscreen.wh.cpp) | Observation externe `SetWinEventHook`, suivi de fenêtres, styles et restauration ; mode outil sans injection. | Confirme l’intérêt du chemin externe. Ce dépôt écrit sa propre gestion d’identité, de journal et de gardien, sans reprendre ses threads ni ses sources. |

La couleur/largueur des bordures, les titres natifs et la transparence proposés par les autres entrées A06 sont traités comme paramètres de ce même configurateur. Les correctifs liés aux thèmes classiques, les compatibilités de rubans et les cadres de Chromium/CEF ne sont pas déclarés obsolètes ni portés sans preuve. Le petit format avancé reste facultatif.

Les métadonnées complètes des mods de la famille, leurs licences signalées et leurs SHA-256 sont conservés dans `evidence/a06-sources.json`. Cela ne constitue ni une certification de licence ni une autorisation de portage des sources.

## API Microsoft et résultat observé

[Les attributs DWM](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute) distinguent préférences de coins, couleurs et matériau système. Les couleurs sont documentées avec le setter ; le matériau système peut être relu. Les attributs récents ont des limites de build, dont 22621 pour SystemBackdrop. Le programme enregistre les résultats réels au lieu de présumer la disponibilité du getter ou du setter.

Sur le host de cette mission, les getters de couleurs 34/35/36 renvoient `E_INVALIDARG`, les getters 20/33/38 réussissent. Le binaire les préserve par défaut et n’autorise leur retour aux défauts Windows que via un réglage explicite. Le succès d’un setter de matériau n’établit pas un flou visible dans un client opaque. Les résultats sont dans `evidence/windows-result.json`.

## Revue de code mort et impact

Périmètre : ce nouveau dépôt, pas un intervalle de commits du core/moteur. Toutes les sources C++ entrent dans la bibliothèque `appearance` puis dans `companion` ou les tests. `wmain`, la classe Win32 de coordinateur et celle de décoration, `WindowEvent`, `Surface::Proc` et `Fixture` sont des points d’entrée dynamiques explicitement enregistrés. Aucun export de DLL n’est déclaré. Le transport, le gardien et le journal sont des contrats externes entre processus propres à ce dépôt.

Les fonctions du parseur et du renderer sont appelées par le runtime et les tests. `Count` appartient au test de lifecycle, `Diagnostics` à l’observation locale, `FromJson/Recover` au gardien, et les fonctions de framing aux processus réels. Le générateur produit les trois fichiers de contrat ; le SDK officiel contrôle le manifeste et les presets. Les champs purement déclaratifs (`label`, `description`, `showIf`, `parent`, limites et options) sont consommés par MyWallpaper et ne sont pas supprimés au motif qu’ils n’ont pas de lecteur C++ direct.

Les helpers JS de capacité privée sont importés par leur preuve d’usage et sont expressément isolés du runtime, pour rendre le besoin commun reviewable ; ils ne constituent pas un backend mort caché dans le binaire. Les notices n’incluent que les dépendances effectivement employées. Les vérifications ciblées n’ont établi aucune voie obsolète justifiant une suppression dans les autres dépôts.

Dans ce nouveau dépôt, l’accesseur `Runtime::Stopped` a été supprimé après recherche exacte : aucun appelant, aucune registration, aucun export ni contrat externe ne l’utilisait. La variable interne de lifecycle reste utilisée. Les callbacks enregistrés et les lecteurs de journal ont été conservés malgré l’absence de références ordinaires pour certains points d’entrée.

L’empreinte des sources locales finales et des dépendances lues est produite dans `evidence/source-hashes.json`. Les fichiers de preuve ne ratifient aucune décision durable.
