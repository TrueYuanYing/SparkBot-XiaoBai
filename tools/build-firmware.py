"""Build with the currently activated ESP-IDF and SparkBot defaults."""
import os,subprocess,sys
from pathlib import Path

root=Path(__file__).resolve().parent.parent
idf=os.environ.get('IDF_PATH')
if not idf:raise SystemExit('Activate ESP-IDF 5.5.3 first (IDF_PATH is not set).')
entry=Path(idf)/'tools/idf.py'
if not entry.is_file():raise SystemExit('Invalid IDF_PATH')
defaults='sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.sparkbot'
subprocess.run([sys.executable,str(entry),'-C',str(root/'firmware'),'-B','build',
                '-D','IDF_TARGET=esp32s3','-D','SDKCONFIG_DEFAULTS='+defaults,'build'],check=True)
