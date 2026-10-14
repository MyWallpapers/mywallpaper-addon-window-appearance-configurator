// Add-on-owned proof of the request shape; no injection or symbol lookup occurs.
export function privateRequest(effect, identity) {
  if(!identity || !Number.isInteger(identity.pid) || identity.pid<=0 || !/^[0-9]+$/.test(identity.creationTime) || !/^[a-f0-9]{64}$/.test(identity.moduleSha256) || !identity.pdbIdentity)throw Error('A measured target and module identity are required');
  const symbol={
    cornerRadius:'private: float __cdecl CTopLevelWindow::GetRadiusFromCornerStyle(void)',
    nativeShadow:'private: static void __cdecl CWindowBorder::GetShadowParameters(enum CWindowBorder::ShadowStyle,int,float *,float *,float *,float *)'
  }[effect];
  if(!symbol)throw Error('Unknown optional private effect');
  return {schemaVersion:1,operation:'resolve-private-symbol',target:{imageName:'dwm.exe',pid:identity.pid,creationTime:identity.creationTime},module:{imageName:'udwm.dll',sha256:identity.moduleSha256,pdbIdentity:identity.pdbIdentity},symbol,deadlineMs:2000};
}
