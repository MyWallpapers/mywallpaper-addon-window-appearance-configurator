import assert from 'node:assert/strict';
import {privateRequest} from '../tools/private-capability.mjs';
const fixture={pid:12345,creationTime:'123456789',moduleSha256:'a'.repeat(64),pdbIdentity:'TEST-FIXTURE-ONLY'};
for(const effect of ['cornerRadius','nativeShadow']){
  const request=privateRequest(effect,fixture);
  assert.equal(request.target.imageName,'dwm.exe');
  assert.equal(request.module.imageName,'udwm.dll');
  assert.equal(request.operation,'resolve-private-symbol');
  assert.ok(request.symbol.includes('C'));
}
assert.throws(()=>privateRequest('cornerRadius',{...fixture,pid:0}));
assert.throws(()=>privateRequest('cornerRadius',{...fixture,moduleSha256:''}));
assert.throws(()=>privateRequest('imaginary',fixture));
console.log('Private request proof passed (fixture data; no DWM operation)');
