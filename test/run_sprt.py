import subprocess
import shlex
import shutil



shutil.copytree("../nnue", "./nnue", dirs_exist_ok=True)


FASTCHESS = "fastchess"

ENGINE_NEW = "../build/gilperchess.exe"
# ENGINE_BASE = "../build/gilperchess.exe"
ENGINE_BASE = "../../gilperchess-master/build/gilperchess.exe"

BOOK = "./books/8moves_v3.pgn"

TC = "1+0.1"

# THREADS = 1
# HASH = 128

ELO0 = 0
ELO1 = 2.5
ALPHA = 0.05
BETA = 0.1

CONCURRENCY = 8
ROUNDS = 10000

cmd = f"""
{FASTCHESS}
-engine name=New cmd={ENGINE_NEW}
-engine name=Base cmd={ENGINE_BASE}
-each proto=uci tc={TC}
-openings file={BOOK} format=pgn order=random
-sprt elo0={ELO0} elo1={ELO1} alpha={ALPHA} beta={BETA}
-rounds {ROUNDS}
-repeat
-concurrency {CONCURRENCY}
-pgnout file="games.pgn"
-log engine=true file="log.txt"
"""

cmd = " ".join(cmd.split())

print("command:")
print(cmd)
print()

process = subprocess.Popen(
    shlex.split(cmd),
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True
)


for line in process.stdout:
    print(line, end="")

process.wait()

print("\nFinished with return code:", process.returncode)
