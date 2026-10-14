import {readFileSync} from 'node:fs';
import {pathToFileURL,fileURLToPath} from 'node:url';
import path from 'node:path';
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const sdkRoot=process.env.MW_CORE_ROOT??path.join(root,'../MyWallpaper');
const {parseAddonManifest,validateSettingValues}=await import(pathToFileURL(path.join(sdkRoot,'packages/sdk-public/dist/addon-schema/index.js')));
const manifest=parseAddonManifest(JSON.parse(readFileSync(path.join(root,'manifest.json'),'utf8')));
for(const preset of manifest.presets){const problems=validateSettingValues(manifest,preset.settings,'device');if(problems.length)throw Error(JSON.stringify(problems));}
console.log(`${manifest.settings.length} definitions and ${manifest.presets.length} presets validated by the current MyWallpaper SDK`);
