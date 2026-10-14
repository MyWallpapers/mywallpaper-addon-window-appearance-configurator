import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const settings = [];
const add = (id, type, label, value, extra = {}) => settings.push({id, type, label, ...(type === 'section' ? {} : {default:value,scope:'device'}), ...extra});
const section = (id, label, description, collapsed=false) => add(id,'section',label,undefined,{description,defaultCollapsed:collapsed});
let parent;
const control = (id,type,label,value,extra={}) => add(id,type,label,value,{parent,...extra});
const select = (id,label,value,options,extra={}) => control(id,'select',label,value,{options:options.map(([value,label])=>({value,label})),...extra});
const range = (id,label,value,min,max,description,extra={})=>control(id,'range',label,value,{min,max,step:1,description,...extra});
const custom={showIf:{setting:'profile',equals:'custom'}};
const profiles = {
  system:{label:'Windows',description:'Conserver les couleurs et matériaux Windows.',dark:{nativeMaterial:'system',nativeColors:false,corner:'system',borderMode:'system',shadowMode:'system',titleMode:'native'},light:{}},
  porcelain:{label:'Porcelaine',description:'Une surface ivoire lumineuse et un liseré sauge.',dark:{nativeMaterial:'opaque',nativeColors:true,corner:'round',borderMode:'solid',borderColor:'#A5B8AC',inactiveBorder:'#586763',captionColor:'#293631',inactiveCaption:'#252C29',textColor:'#F1F5F0',inactiveText:'#B8C5BD',titleStart:'#293631',titleEnd:'#293631',titleFill:'solid',shadowMode:'soft',shadowColor:'#071710',shadowOpacity:30,titleMode:'companion'},light:{borderColor:'#82968B',inactiveBorder:'#CED8D1',captionColor:'#F5F4EC',inactiveCaption:'#E9EDE6',textColor:'#263C32',inactiveText:'#556A5F',titleStart:'#F5F4EC',titleEnd:'#F5F4EC'}},
  nocturne:{label:'Nocturne',description:'Bleu nuit, titre centré et contour cobalt.',dark:{nativeMaterial:'mica',nativeColors:true,corner:'round',borderMode:'solid',borderColor:'#6889E8',inactiveBorder:'#354461',captionColor:'#1C2740',inactiveCaption:'#192131',textColor:'#F1F5FF',inactiveText:'#A8B7D3',titleStart:'#1C2740',titleEnd:'#253755',titleFill:'gradient',titleAlign:'center',shadowMode:'soft',shadowColor:'#070E25',shadowOpacity:55,titleMode:'companion'},light:{captionColor:'#E0E7F5',inactiveCaption:'#E9EDF4',textColor:'#192A48',inactiveText:'#516386',titleStart:'#E0E7F5',titleEnd:'#C3D4EF'}},
  aurora:{label:'Aurore',description:'Un dégradé profond vert et bleu, une bordure irisée.',dark:{nativeMaterial:'acrylic',nativeColors:true,corner:'round',borderMode:'gradient',borderColor:'#66D8CA',borderEnd:'#748FF4',inactiveBorder:'#3F626B',captionColor:'#183E43',inactiveCaption:'#1D2E39',textColor:'#EFFDFB',inactiveText:'#A1C5CF',titleFill:'gradient',titleStart:'#183E43',titleEnd:'#343C68',titleAlign:'center',shadowMode:'soft',shadowColor:'#071D28',shadowOpacity:50,titleMode:'companion'},light:{titleStart:'#D3F0E8',titleEnd:'#DCE3FA',textColor:'#183844',inactiveText:'#466574',captionColor:'#D3F0E8',inactiveCaption:'#E5EEEF',borderColor:'#368D7E',borderEnd:'#6376CA'}},
  ember:{label:'Braise',description:'Cuivre, brun profond et typographie chaleureuse.',dark:{nativeMaterial:'opaque',nativeColors:true,corner:'small',borderMode:'solid',borderColor:'#D59B65',inactiveBorder:'#735848',captionColor:'#3C2A23',inactiveCaption:'#302824',textColor:'#FFF2DD',inactiveText:'#D4BDA6',titleStart:'#3C2A23',titleEnd:'#68402C',titleFill:'gradient',titleFont:'Georgia',shadowMode:'soft',shadowColor:'#1B0903',shadowOpacity:48,titleMode:'companion'},light:{captionColor:'#F4E5D3',inactiveCaption:'#EEE8DE',textColor:'#4C2D1F',inactiveText:'#795B49',titleStart:'#F4E5D3',titleEnd:'#E5C7A1'}},
  graphite:{label:'Graphite',description:'Un cadre carré sobre et contrasté pour travailler.',dark:{nativeMaterial:'opaque',nativeColors:true,corner:'square',borderMode:'solid',borderColor:'#9AA5B1',inactiveBorder:'#414852',captionColor:'#22272E',inactiveCaption:'#202429',textColor:'#F5F7FA',inactiveText:'#B3BDC9',titleStart:'#22272E',titleEnd:'#22272E',titleFill:'solid',shadowMode:'system',titleMode:'companion'},light:{borderColor:'#657180',inactiveBorder:'#B4BDC8',captionColor:'#F0F2F5',inactiveCaption:'#E9ECF0',textColor:'#222C38',inactiveText:'#566577',titleStart:'#F0F2F5',titleEnd:'#F0F2F5'}},
  frost:{label:'Givre',description:'Matériau acrylique natif et contours discrets.',dark:{nativeMaterial:'acrylic',nativeColors:true,corner:'round',borderMode:'solid',borderColor:'#A7C4D5',inactiveBorder:'#456075',captionColor:'#273D4A',inactiveCaption:'#26333C',textColor:'#F0FAFF',inactiveText:'#B0C7D5',titleMode:'native',shadowMode:'soft',shadowColor:'#0D2634',shadowOpacity:38},light:{captionColor:'#E3EFF6',inactiveCaption:'#E9EEF2',textColor:'#243E4D',inactiveText:'#506B7A',borderColor:'#7199AE'}}
};
section('general','Commencer','Choisissez un profil et les applications à habiller.'); parent='general';
control('enabled','boolean','Activer l’apparence des fenêtres',true);
select('profile','Profil visuel','nocturne',[...Object.entries(profiles).map(([id,p])=>[id,p.label]),['custom','Personnalisé']],{description:'Les profils règlent les couleurs et la décoration. Personnalisé déverrouille tous les détails.'});
control('targetApps','textarea','Applications choisies','notepad.exe',{description:'Un nom d’application par ligne, par exemple notepad.exe ou code.exe. Une liste vide ne modifie aucune application.'});
control('excludedApps','textarea','Applications à préserver','',{description:'Un nom par ligne. Ces exclusions prennent toujours le dessus.'});
control('includeDialogs','boolean','Inclure les fenêtres de dialogue',false,{description:'Habille aussi les dialogues appartenant aux applications choisies.'});
select('monitor','Écrans concernés','all',[['all','Tous les écrans'],['primary','Écran principal'],['secondary','Autres écrans']]);
select('theme','Ambiance','auto',[['auto','Suivre le thème Windows'],['dark','Sombre'],['light','Claire']],{description:'Les profils disposent de palettes claires et sombres. Les couleurs personnalisées restent celles que vous choisissez.'});
section('materialSection','Matériau et transparence','Le matériau natif dépend de Windows et du rendu de l’application.'); parent='materialSection';
select('nativeMaterial','Matériau Windows','mica',[['system','Conserver celui de l’application'],['opaque','Opaque'],['mica','Mica · teinté par le fond'],['acrylic','Acrylique · verre flouté'],['tabbed','Mica profond']],{...custom,description:'Mica et acrylique exigent Windows 11 22621 ou ultérieur. Le contenu opaque d’une application peut les masquer.'});
control('nativeColors','boolean','Colorer le cadre natif',true,custom);
control('restoreNativeDefaults','boolean','Appliquer les effets natifs non relisibles',false,{description:'Windows ne permet pas de relire certains attributs. Si activé, leurs couleurs, thème et coins reviennent aux valeurs Windows à l’arrêt, sans garantie de retrouver un style antérieur de l’application. Sinon ils sont préservés.'});
range('windowOpacity','Opacité de la fenêtre (%)',100,35,100,'Affecte aussi le contenu. Les fenêtres déjà gérées par un autre moteur de transparence sont préservées.');
range('inactiveOpacity','Opacité à l’arrière-plan (%)',100,35,100,'La fenêtre active retrouve son opacité de premier plan.');
control('preserveMaximized','boolean','Préserver les coins en plein écran et maximisés',true,{description:'Suspend les découpes de coins et les ombres supplémentaires quand elles gênent les bords de l’écran.'});
section('borderSection','Contours et coins','Un liseré discret ou un cadre expressif.'); parent='borderSection';
settings.at(-1).showIf=custom.showIf;
select('borderMode','Bordure','solid',[['system','Windows'],['none','Masquer le contour natif'],['solid','Couleur unie'],['gradient','Dégradé']],custom);
range('borderWidth','Épaisseur du liseré ajouté (px)',2,0,12,'Le liseré est dessiné à l’extérieur. Il ne réduit pas la zone utilisable.',custom);
control('borderColor','color','Bordure active','#6889E8',custom);
control('borderEnd','color','Bordure · couleur d’arrivée','#8DCDE7',{...custom,description:'Couleur d’arrivée du dégradé du liseré.'});
control('inactiveBorder','color','Bordure inactive','#354461',custom);
select('gradientDirection','Direction des dégradés','horizontal',[['horizontal','De gauche à droite'],['vertical','Du haut vers le bas'],['diagonal','En diagonale']],custom);
select('corner','Forme des coins','round',[['system','Windows'],['square','Droits'],['small','Arrondis discrets'],['round','Arrondis Windows'],['custom','Rayon précis · découpe de fenêtre']],custom);
range('cornerRadius','Rayon de la découpe (px)',16,0,48,'La découpe peut rogner les coins du contenu. Elle remplace temporairement la région de fenêtre.',{showIf:{setting:'corner',equals:'custom'}});
section('titleSection','Barre de titre','Le titre compagnon se place au-dessus de la fenêtre, sans recouvrir son contenu.'); parent='titleSection';
select('titleMode','Présentation du titre','companion',[['native','Titre Windows / application'],['companion','Barre de titre compagnon'],['hidden','Masquer le titre Windows']],custom);
select('titleFill','Fond du titre compagnon','gradient',[['solid','Uni'],['gradient','Dégradé'],['translucent','Translucide']],custom);
control('titleStart','color','Titre · couleur de départ','#1C2740',custom);
control('titleEnd','color','Titre · couleur d’arrivée','#253755',custom);
range('titleOpacity','Opacité du titre compagnon (%)',100,40,100,'Le mode translucide laisse voir le bureau derrière le titre ; ce réglage ne crée pas de flou.',custom);
control('captionColor','color','Titre natif actif','#1C2740',custom);
control('inactiveCaption','color','Titre natif inactif','#192131',custom);
control('textColor','color','Texte du titre actif','#F1F5FF',custom);
control('inactiveText','color','Texte du titre inactif','#A8B7D3',custom);
select('titleAlign','Alignement du texte','center',[['left','À gauche'],['center','Centré'],['right','À droite']],custom);
select('titleFont','Police du titre','Segoe UI',[['Segoe UI','Segoe UI'],['Calibri','Calibri'],['Georgia','Georgia']],custom);
range('titleSize','Taille du texte (px)',13,10,24,'Taille indépendante du facteur d’échelle de l’écran.',custom);
control('titleBold','boolean','Titre en gras',false,custom);
range('titleHeight','Hauteur du titre compagnon (px)',34,26,64,'Le titre reste à l’extérieur de la zone de contenu.',custom);
range('titlePadding','Marge du texte (px)',12,4,32,'Espace entre le titre et les côtés.',custom);
select('titleText','Texte du titre','application',[['application','Titre de l’application'],['custom','Texte personnalisé'],['hidden','Sans texte']]);
control('customTitle','string','Texte personnalisé','Ma fenêtre',{showIf:{setting:'titleText',equals:'custom'}});
control('titleIcon','boolean','Afficher l’icône du titre compagnon',true);
section('buttonSection','Boutons de fenêtre','Les boutons du titre compagnon commandent la fenêtre sélectionnée.'); parent='buttonSection';
select('buttonStyle','Style des boutons','line',[['line','Traits fins'],['circles','Pastilles colorées'],['compact','Compact']],custom);
select('buttonSide','Position des boutons','right',[['right','À droite'],['left','À gauche']],custom);
range('buttonSize','Largeur des boutons (px)',36,24,56,'Les zones de clic suivent la mise à l’échelle.',custom);
control('buttonMinimize','boolean','Bouton Réduire',true);
control('buttonMaximize','boolean','Bouton Agrandir / restaurer',true);
control('buttonClose','boolean','Bouton Fermer',true,{description:'Masquer le bouton compagnon conserve Alt+F4 et le menu système Windows.'});
control('buttonHover','color','Fond au survol des boutons','#3D5576',custom);
control('closeHover','color','Fond au survol de Fermer','#B83A45',custom);
section('shadowSection','Relief','Les ombres ajoutées ne remplacent pas les ombres natives de DWM.'); parent='shadowSection';
settings.at(-1).showIf=custom.showIf;
select('shadowMode','Ombre supplémentaire','soft',[['system','Windows uniquement'],['none','Aucune ombre ajoutée'],['soft','Ombre douce']],custom);
control('shadowColor','color','Couleur de l’ombre','#070E25',custom);
range('shadowOpacity','Intensité de l’ombre (%)',45,0,80,'Opacité maximale de l’ombre supplémentaire.',custom);
range('shadowSize','Étendue de l’ombre (px)',24,0,64,'Distance de diffusion autour de la fenêtre.',custom);
range('shadowBlur','Douceur de l’ombre (px)',16,1,40,'Plus la valeur augmente, plus le bord est diffus.',custom);
range('shadowOffset','Décalage vers le bas (px)',8,-20,32,'Déplace le relief verticalement.',custom);
section('frameSection','Cadre et modes','Les cadres personnalisés de certaines applications peuvent ignorer les styles Windows.',true); parent='frameSection';
select('frameMode','Mode de fenêtre','normal',[['normal','Conserver le cadre'],['native','Demander un cadre Windows'],['borderless','Sans bordure'],['fullscreen','Plein écran fenêtré']],{description:'Le plein écran utilise l’écran de la fenêtre. Quitter ce mode restaure la position si vous ne l’avez pas déplacée entre-temps.'});
control('nativeResize','boolean','Autoriser le redimensionnement du cadre demandé',true,{showIf:{setting:'frameMode',equals:'native'}});
section('rulesSection','Profils par application','Trois exceptions accessibles sans code ; la première règle correspondante gagne.',true); parent='rulesSection';
for(let n=1;n<=3;n++){
  control(`rule${n}Enabled`,'boolean',`Activer la règle ${n}`,false);
  control(`rule${n}Apps`,'textarea',`Règle ${n} · applications`,'',{showIf:{setting:`rule${n}Enabled`,equals:true},description:'Un nom .exe par ligne. Ces applications rejoignent aussi la sélection.'});
  select(`rule${n}Profile`,`Règle ${n} · profil`,'frost',Object.entries(profiles).map(([id,p])=>[id,p.label]),{showIf:{setting:`rule${n}Enabled`,equals:true}});
  range(`rule${n}Opacity`,`Règle ${n} · opacité (%)`,100,35,100,'Opacité pour cette application, active ou inactive.',{showIf:{setting:`rule${n}Enabled`,equals:true}});
}
section('advancedSection','Affiner la sélection','Facultatif. Les effets principaux restent disponibles dans les sections précédentes.',true); parent='advancedSection';
control('advancedRules','textarea','Exceptions avancées (JSON)','',{description:'Facultatif : schemaVersion 1, rules [{app:"app.exe",class:"Classe exacte",titleContains:"Texte",profile:"frost",opacity:90,exclude:false}]. Maximum 64 règles.'});
const manifest={runtime:'native-v1',name:'Apparence des fenêtres',description:'Profils clairs et sombres, cadres dégradés, titres, ombres et règles par application pour Windows.',version:'0.1.1',deviceSettingsVersion:'1',settings,presets:Object.entries(profiles).map(([id,p])=>({id,name:p.label,settings:{profile:id}})),native:{companion:{runtime:'process-v2',entries:{'windows-x86_64':'native/out/windows-x86_64/companion.exe'}}}};
if(settings.length>100)throw Error(`${settings.length} settings exceed DEC-0012`);
const literal=value=>JSON.stringify(value).match(/[\s\S]{1,3500}/gu).map(chunk=>`R"appearance(${chunk})appearance"`).join('\n');
const outputs={'manifest.json':JSON.stringify(manifest,null,2)+'\n','profiles/profiles.json':JSON.stringify(profiles,null,2)+'\n','src/settings_generated.hpp':`// Generated by tools/generate.mjs. Do not edit.\n#pragma once\nnamespace appearance {\ninline constexpr char kManifest[] = ${literal(manifest)};\ninline constexpr char kProfiles[] = ${literal(profiles)};\n}\n`};
for(const [file,content]of Object.entries(outputs)){const dest=path.join(root,file);if(process.argv.includes('--check')){if(!fs.existsSync(dest)||fs.readFileSync(dest,'utf8')!==content)throw Error(`${file} is stale`);}else{fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,content);}}
console.log(`${settings.length} definitions, ${settings.filter(s=>s.type!=='section').length} values, ${Object.keys(profiles).length} profiles`);
