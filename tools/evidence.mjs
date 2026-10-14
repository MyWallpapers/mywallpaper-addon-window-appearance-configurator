import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const files=[];
function walk(directory){for(const entry of fs.readdirSync(path.join(root,directory),{withFileTypes:true})){const relative=path.join(directory,entry.name);if(entry.isDirectory())walk(relative);else if(entry.isFile())files.push(relative.replaceAll('\\','/'));}}
for(const directory of ['src','include','tests','tools','profiles','contracts','docs','notices','third_party'])walk(directory);
files.push('.gitattributes','.gitignore','manifest.json','mywallpaper.config.json','CMakeLists.txt','README.md','LICENSE','THIRD_PARTY_NOTICES.md','native/build.ps1','evidence/a06-sources.json');
const coreRoot=process.env.MW_CORE_ROOT??path.join(root,'../MyWallpaper');
const record={date:new Date().toISOString(),scope:'exact-source-qualification-evidence-not-a-decision',sources:Object.fromEntries(files.sort().map(file=>[file,digest(path.join(root,file))])),dependencies:{jsonVersion:'3.12.0',jsonSha256:digest(path.join(root,'third_party/nlohmann/json.hpp')),ledgerSha256:digest(path.join(coreRoot,'docs/decisions/decision-ledger.json')),sdkValidatorSha256:digest(path.join(coreRoot,'packages/sdk-public/dist/generated/addon-manifest-validator.js'))},binary:{path:'native/out/windows-x86_64/companion.exe',sha256:digest(path.join(root,'native/out/windows-x86_64/companion.exe'))},results:{windows:JSON.parse(fs.readFileSync(path.join(root,'evidence/windows-result.json'),'utf8')),wire:JSON.parse(fs.readFileSync(path.join(root,'evidence/wire-result.json'),'utf8'))}};
fs.writeFileSync(path.join(root,'evidence/source-hashes.json'),JSON.stringify(record,null,2)+'\n');
console.log(`${files.length} local source hashes; ${record.results.wire.count} integration checks; binary sha256:${record.binary.sha256}`);
