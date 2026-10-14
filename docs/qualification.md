# Qualification locale

Périmètre du 13 octobre 2026 : Windows 11 Professionnel x64 **build 26200**, MSVC 19.44, Windows SDK 10.0.26100.0, Release. La référence MyWallpaper est `299548ebcc20770fd73b10bdffd5f05586fbe126` et le ledger SHA-256 est `ed477485a495b1e6ff9e931dfae5378461378c4c57977a4e436e753ca9ddba6c`. Le SDK de moteur lu est `ff32c520dd4ccd9073c9dc001a180e886d50ab63`. Aucune modification du core, moteur, ledger ou autres add-ons n’est nécessaire pour ce chemin externe.

Le bundle local est `native/out/windows-x86_64/companion.exe`, accompagné des licences. Le manifeste est accepté par `parseAddonManifest` et chaque preset par `validateSettingValues` du SDK MyWallpaper courant : 77 définitions, 68 valeurs et sept presets. `tools/generate.mjs --check` vérifie la correspondance entre manifeste, profils et contrat C++ généré.

## Preuves

- `appearance_contract` : 41 assertions, validation de bornes/types, rejet des sélections larges et champs inconnus, priorité des règles, exclusions, profils clair/sombre, contrôle de l’intérieur transparent du cadre, zones de bouton, framing init et renderer de titre. Il s’agit de contrats et de rendu, sans certification applicative.
- `appearance_windows` : 19 assertions sur une fenêtre Win32 réelle créée par le test : requêtes DWM acceptées, alpha, région, styles, round-trip du journal, identité de fenêtre, changement de profil, désactivation, restauration, plein écran, conservation de l’alpha modifié par un autre propriétaire et reprise de journal. `evidence/windows-result.json` garde les probes DWM et diagnostics.
- `tests/integration.py` : companion réel et deux processus Win32 distincts, dont une copie renommée de la fixture. Framing split, profil/opacité séparé, singleton, rejet de settings sans perdre le profil courant, boutons compagnons, conservation du cadre maximisé, retour après réduction, désactivation, shutdown, EOF, destruction forcée du companion et restauration par gardien. Le résultat final et le nombre d’assertions sont dans `evidence/wire-result.json`.
- `tests/private-capability.test.mjs` : construction et refus des demandes de résolution privée avec identités de fixture ; aucun accès à DWM. Ce n’est pas un test d’un résolveur existant.
- `evidence/profiles-preview.png` : cinq styles de titre compagnon, clair et sombre, issus du renderer livré. Les autres PNG présentent leurs palettes individuelles. Ces fichiers ne sont pas des captures d’applications tierces ni des simulations de flou natif.

L’intégration utilise uniquement ses propres hosts et ne termine pas un processus utilisateur. Elle ne tente pas d’injection dans Explorer ni dans DWM et ne modifie pas les couleurs système.

## Limites concrètes

| À qualifier | Preuve manquante |
| --- | --- |
| Applications tierces dans le Desktop | Le contrôle complémentaire du 14 octobre a vérifié l'activation MyWallpaper, cinq arrêts/reprises et le retrait sur un host Win32 distinct ; l'ergonomie et les effets sur des applications tierces restent à qualifier. |
| Notepad / applications Win32 tierces | Captures avant/après dans les vrais hosts, contenu/client préservé, changements de titre, stabilité de styles et redémarrage de l’application. |
| Chromium / Electron / WinUI / Office | Cadre personnalisé, création tardive, nonclient/hit-test, intégration du cadre demandé et interactions avec les onglets. Les styles seuls peuvent être ignorés. |
| Mica / acrylique réellement visible | Host avec surface compatible et captures/observation du matériau ; les tests établissent le succès des setters, pas un flou universel. |
| Windows 10 19045 et autres builds Windows 11 | API rejetées sans effets résiduels, fallback externe, reprise et boutons dans ces builds réels. |
| Multi-écran / DPI / snapping | Écrans physiques différents, 100/125/150/200 %, déplacements, zones d’accrochage et changements de moniteur. Les rendus à 125 % ne remplacent pas cette validation. |
| Occlusion / plein écran / bureaux virtuels | Cohérence de z-order des surfaces externes, fenêtre topmost, bureau cloaked, retours de sommeil/verrouillage. |
| Accessibilité et usage | Lecteurs d’écran pour les boutons dessinés, Alt+Espace/Alt+F4, chemins clavier des applications et titres très longs/RTL. |
| Performances | Mesures CPU/GPU/mémoire sur beaucoup de fenêtres, grand écran/HDR ; les bitmaps sont bornées mais aucune mesure de charge générale n’est annoncée. |
| Restitution après défaillance combinée | Gardien et companion simultanément perdus, disque/journal indisponible ou application refusant la restauration. Les journaux non restaurés restent une preuve et un point de reprise. |
| Rayons/ombres DWM arbitraires | Résolveur privé absent du SDK actuel et vérification spécifique de symboles/ABI/offsets par build. Non livré. |

Les callbacks WinEvent déposent une notification ; ils n’exécutent pas de RPC dans un host applicatif. Toutes les mutations et la composition s’effectuent sur le thread du companion. Le journal est remplacé atomiquement et les verrous transitoires de partage font l’objet d’une reprise bornée. Les handles PID/date de création et le jeton de fenêtre protègent la restitution d’une identité recyclée. Les couleurs DWM non relisibles sont une exception explicite documentée dans le README.

Le mandat de développement est exécuté localement ; il ne constitue pas une admission, une publication ou une nouvelle décision de lancement.
