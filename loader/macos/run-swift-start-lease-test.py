from pathlib import Path
import subprocess
source=Path(__file__).resolve().parent
work=source.parents[3]/'work/swift-start-lease';work.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang','-Wall','-Wextra','-Werror','-c',str(source/'swift_start_scope_fixture.c'),'-o',str(work/'scope.o')],check=True)
subprocess.run(['xcrun','swiftc','-parse-as-library','-import-objc-header',str(source/'swift_network_gate.h'),str(source/'swift_network_gate.swift'),str(source/'swift_start_lease_test.swift'),str(work/'scope.o'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test'),str(work/'results.json')],check=True,timeout=15)
