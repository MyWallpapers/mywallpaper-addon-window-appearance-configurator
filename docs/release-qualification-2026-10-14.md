# Contrôle de publication 0.1.0 / 0.1.1

Preuve d'implémentation consommable du 14 octobre 2026, sans nouvelle décision. Sources examinées : `dcf3139051802056669434dc3eb6be91dda326b9`. Ledger MyWallpaper courant SHA-256 : `5c198b56bd404e3e035f31687e2ed23b7c0d87655e998bd0940fa2649ddae993` ; référence core `299548ebcc20770fd73b10bdffd5f05586fbe126`. Les commits suivants de qualification ne modifient pas le comportement livré.

## Conformité et preuves

| Référence | Classification | Preuve vérifiée |
| --- | --- | --- |
| DEC-0012 | Conforme | 77 définitions, dont 68 valeurs et neuf sections ; sept profils validés par le SDK officiel. |
| DEC-0099 | Conforme | Licence MIT du dépôt et du JSON effectivement compilé ; notices de dépendances conservées. Publication demandée par le propriétaire. |
| DEC-0152 | Conforme au périmètre local | Version 0.1.0 exacte, empreinte du companion et résultats de qualification ci-dessous. L'admission et la reconstruction centrales doivent encore vérifier leur propre paquet. |
| DEC-0160 | Conforme | Aucun code Windhawk incorporé. Aucune implémentation privée du moteur nécessaire pour ce companion externe Win32/DWM. |

Le build Release x64 sur Windows 11 26200, MSVC 19.44 et SDK 10.0.26100 passe les 41 assertions de contrat, 19 assertions Windows et 36 contrôles process-v2. Ceux-ci emploient deux processus Win32 de test indépendants. Ils couvrent les boutons Réduire/Agrandir/Fermer, le refus d'une seconde instance, les réglages invalides, la désactivation, shutdown, EOF et la restauration par gardien après destruction forcée du propriétaire. Aucune fenêtre utilisateur n'est terminée par ces tests.

L'intégration réelle MyWallpaper a vérifié l'application des réglages, cinq cycles de désactivation/réactivation, le retour au titre natif et le retrait du calque avec restauration exacte de sa fenêtre de test. Le nouveau build autonome produit exactement le même companion que ce test : SHA-256 `d019333d8b5cb5e96a8b8b77c831a5093bd868f08054fde8bdb71b00434bf413`. Cette égalité ne remplace pas le contrôle des binaires reconstruits par le serveur.

`evidence/source-hashes.json` contient les empreintes des sources et les résultats locaux HWND/process-v2. `thumbnail.png` est une sortie du renderer réellement livré ; il ne simule pas le flou DWM. Le build central exécute les contrats avec `-SkipHostQualification` et ne présente pas un runner CI comme une qualification visuelle du bureau de l'utilisateur.

## Corrections et impact

- Le SDK moteur était lié sans aucun appel à son API. Les références et includes des sources n'utilisent que Windows et nlohmann/json. Sa suppression du build est compensée par les mêmes définitions de compilation Windows et le vendoring exact du JSON 3.12.0 sous MIT. Le binaire est inchangé ; aucun export ou contrat dynamique du moteur n'est supprimé.
- La configuration centrale ne contient que les champs communs aux validateurs actuels ; le champ optionnel `native.builds[].inputs` n'est pas requis. La validation SDK autonome reste disponible dans `tools/validate-manifest.mjs`, utilisée explicitement avant publication ; elle n'est pas supprimée comme code mort.
- Le build initial limitait Visual Studio à la génération 17. Le runner central du 14 octobre ne la fournit pas : son build a échoué avant compilation. La 0.1.1 sélectionne désormais le générateur 17/2022 ou 18/2026 correspondant à l'installation détectée et fixe explicitement cette instance CMake. Les paramètres existants du script sont conservés ; `SkipHostQualification` est un ajout optionnel utilisé par le build central. Cette correction est limitée au build ; le tag 0.1.0 refusé reste immuable.
- Le test des trois boutons attend désormais le titre compagnon visible après les changements d'état. Les assertions de fonctionnalité restent inchangées ; la course du test n'était pas une preuve de panne du runtime.
- Les entrées auteur de génération, validation et collecte de preuve restent documentées. L'ancienne notice MIT du SDK public est conservée pour l'étude historique, sans attribuer cette licence à l'implémentation actuelle du moteur.

Aucun changement du core MyWallpaper, du moteur, de schéma partagé ni du ledger. Les répertoires de build, tokens de développement et sorties temporaires ne font pas partie du dépôt publié.

## Limites

Ces contrôles ne certifient pas tous les cadres Chromium/WinUI/Office, le flou visible sur une application tierce, les lecteurs d'écran, Windows 10 ou le multi-écran physique. Les attributs DWM non relisibles sont préservés par défaut ; leur override explicite ne permet qu'un retour aux valeurs Windows. Les ombres sont additionnelles et le titre compagnon translucide n'est pas un flou du bureau. Les performances sur un grand nombre de fenêtres et la rétention mémoire de longue durée restent à mesurer : aucun résultat de stress général n'est revendiqué.

## Recontrôle 0.1.1

Source examinée : `69927cf` (commit complet dans la provenance Git du tag). Même ledger SHA-256 ci-dessus. Après correction du générateur Visual Studio, deux nouveaux clones Git et répertoires de compilation locaux donnent le même companion SHA-256 `93cf281a3fcaca7e8bf343e00ad0454f730a4962f4613467426c188992335861`. La différence avec la 0.1.0 vient du manifeste de version embarqué ; le code du runtime n'est pas modifié. Les 41 assertions de contrat, 19 assertions Windows et 36 contrôles process-v2 passent de nouveau. Le test process-v2 lit désormais la version du manifeste plutôt qu'une version de release codée dans sa fixture ; les exemples de validation SemVer restent inchangés.

Observation complémentaire de la 0.1.0 avant ce changement de version : 100 désactivations/réactivations restaurent exactement le style et le mode alpha à chaque arrêt ; shutdown supprime le journal. Mémoire privée du companion : 2 801 664 octets au départ, 2 998 272 après 100 cycles, 4 440 064 en fin d'observation de 99 secondes. Handles 126 → 129 ; objets GDI 3 → 3 ; objets USER 10/11 actifs → 8 désactivés, sans croissance par cycle. CPU variable entre les trois périodes de 30 secondes : environ 5,04 / 7,61 / 0,31 secondes CPU. L'activité du bureau n'a pas été isolée : ces chiffres ne permettent pas d'annoncer un coût CPU constamment faible ou une absence de fuite sur une journée. Le gardien et les autres processus ne sont pas inclus dans cette mesure.
