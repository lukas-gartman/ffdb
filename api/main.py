from ffdbcore import FFDB
import os

db = FFDB("python-test.db", os.O_CREAT | os.O_RDWR, 0o600, 1024)
db.hello()