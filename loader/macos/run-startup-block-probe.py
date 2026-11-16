import json,os,pathlib,subprocess,time
root=pathlib.Path(__file__).resolve().parent
app=pathlib.Path(os.environ.get('LOADER_PROBE_APP',str(root/'experimental/SV2 Loader Probe.app')))
env=os.environ.copy();env.pop('DYLD_INSERT_LIBRARIES',None);env.pop('LOADER_PROBE_HTTP_BASE',None);env['LOADER_PROBE_BLOCK_HOST_AT_START']='1'
with (root/'startup-block.stdout').open('wb') as out,(root/'startup-block.stderr').open('wb') as err:
 p=subprocess.Popen([str(app/'Contents/MacOS/loader-launcher')],env=env,stdout=out,stderr=err)
 time.sleep(4);alive=p.poll() is None
 log=(root/'startup-block.stderr').read_text(errors='replace')
 children=subprocess.run(['pgrep','-P',str(p.pid)],capture_output=True,text=True).stdout.split()
 if alive:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
markers=[line for line in log.splitlines() if 'LOADER_' in line or 'NETWORK_GATE' in line]
denied=[line for line in markers if 'source=host destination=dreamtonics.com' in line and 'deny=1' in line]
report={'pid':p.pid,'alive_at_4s':alive,'children':children,'markers':markers,'actual_host_official_requests_intercepted_and_denied':bool(denied),'scope':'Request cancellation observed at NSURLSession resume; packet-level absence not measured; complete original-host network coverage remains open.'}
(root/'official-startup-block-results.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
