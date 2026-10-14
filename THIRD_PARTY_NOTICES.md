# Contenu effectivement employé

Les sources originales de l’add-on sont sous MIT (LICENSE).

- JSON for Modern C++, version 3.12.0, Niels Lohmann, MIT : header conservé dans `third_party/nlohmann/json.hpp`, compilé dans l’exécutable. Notice intégrale : `notices/nlohmann-json.LICENSE.MIT`.
- Le contrat public `process-v2` du SDK MyWallpaper a servi à l’étude du transport. Aucun code d’implémentation du Native Engine n’est compilé ou distribué par cet add-on. La notice de sa révision MIT historiquement étudiée est conservée dans `notices/native-engine.LICENSE.MIT` ; elle ne décrit pas la licence actuelle du moteur.
- Le configurateur taskbar MIT a servi de référence de présentation et de lifecycle ; ses sources ne sont pas copiées dans ce dépôt.

Windows SDK, DWM, GDI+, User32, Shell32 et Advapi32 sont des API/implémentations système, pas des binaires tiers copiés dans le bundle. Le companion n’inclut ni Windhawk, ni MinHook, ni crates Rust du contrôleur partagé. L’étude fonctionnelle des mods Windhawk ne change pas leurs licences. Le catalogue, les sources épinglées et les signalements de licence sont référencés dans `docs/source-study.md` ; aucun texte de ces mods n’est incorporé au runtime.
