# Variantes privées non livrées

Ce document décrit des besoins techniques constatés, pas une décision de produit ni une extension du core. Le ledger lu pour cette mission conserve DEC-0012/0013/0099/0157/0160 ; il n’est pas modifié.

Le chemin livré fonctionne hors des applications. Deux demandes A06 dépassent les API externes et ne sont pas présentées comme réalisées : un rayon arbitraire du compositor avec ombres natives correspondantes, et le remplacement garanti d’un cadre personnalisé d’application par le cadre natif.

Pour le chemin DWM, le besoin minimal générique est de résoudre une adresse de symbole privé **sur un processus exact et un module exact**. La demande JSON proposée dans `contracts/private-symbol-request.schema.json` contient PID/date de création, nom du module, SHA-256 de l’image et identité PDB vérifiée, nom de symbole et délai. La réponse future doit fournir une adresse validée dans ce module ou une erreur explicite (`unsupported-build`, `identity-mismatch`, `not-found`, `timeout`), sans chargement de PDB ni RPC dans un callback du compositor. Hook, désactivation, drain et propriété technique réutiliseraient ensuite les garanties du moteur ; radius, ombres, couleurs et paramètres restent aux add-ons.

`tools/private-capability.mjs` construit deux demandes précises pour des symboles observés dans les sources épinglées : le rayon et les paramètres d’ombre. `node tests/private-capability.test.mjs` prouve l’usage du format avec des identités de fixture et refuse les identités manquantes. Ce test ne résout aucune adresse et ne contacte pas DWM. Les symboles cités ont des signatures/visibilités qui varient selon le build ; une demande ne constitue pas une allowlist de compatibilité.

`MwHostApi` v1 expose `resolve_export`, puis `hook_create/enable/disable/remove` sur des adresses. Elle n’expose pas ce résolveur PDB ni une identité d’instance multi-processus. Aucun fichier du moteur n’est modifié ici.

Pour les cadres Chromium/WinUI/Office, la bibliothèque de surface doit être attachée dans chaque application sélectionnée, observer les créations de fenêtres et tenir compte de leurs messages de calcul/hit-test nonclient. Le moteur doit seulement garantir l’attachement à un PID/date de création donné, l’observation de la disparition du host et un lifecycle coopératif ; la sélection de classes, les messages à adapter et le rendu ne deviennent pas des règles métier du moteur. Le configurateur actuel utilise les styles externes et un titre compagnon ; le contrôle « Demander un cadre Windows » reste une demande que l’application peut ignorer.

Les hosts applicatifs et DWM ne sont pas des chemins interchangeables. La qualification d’un hook de fenêtre applicative ne prouve ni la sûreté ni la compatibilité du chemin DWM. Aucun adaptateur privé factice n’est chargé, aucune injection dans DWM n’est tentée, et les animations de fenêtre restent dans A07.
