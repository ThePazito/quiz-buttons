import time
import sqlite3
import serial

# === CONFIG ===
PORT = "/dev/ttyACM0"  # adjust as needed
BAUD = 9600
DB_PATH = "/home/admin/Desktop/Quiz_Buttons/buzzer_game.db"
# ==============

# --- Setup serial ---
ser = serial.Serial(PORT, BAUD, timeout=0.2)
time.sleep(2)

# --- Setup database ---
con = sqlite3.connect(DB_PATH)
cur = con.cursor()

# Create tables
cur.execute("""
    CREATE TABLE IF NOT EXISTS rounds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    started_at TEXT NOT NULL
)
""")
cur.execute("""
    CREATE TABLE IF NOT EXISTS buzzes (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        round_id INTEGER NOT NULL,
        team INTEGER NOT NULL,
        timestamp REAL NOT NULL,
        UNIQUE(round_id, team),
        FOREIGN KEY (round_id) REFERENCES rounds(id)
    )
""")
con.commit()

# --- Helper functions ---
def start_new_round():
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    cur.execute("INSERT INTO rounds (started_at) VALUES (?)", (ts,))
    con.commit()
    return cur.lastrowid

def add_buzz(round_id, team):
    ts = time.time() 
    try:
        cur.execute("INSERT INTO buzzes (round_id, team, timestamp) VALUES (?, ?, ?)",
                    (round_id, team, ts))
        con.commit()
        print("|")
        print(f"| [BUZZ] — Team {team} saved")
    except sqlite3.IntegrityError:
        print(f"[DUPLICATE] — Team {team} already buzzed")

def get_order(round_id):
    rows = cur.execute(
        "SELECT team FROM buzzes WHERE round_id = ? ORDER BY timestamp ASC", (round_id,)
    ).fetchall()
    return [r[0] for r in rows]

def send_order(order):
    msg = "ORDER:" + ",".join(map(str, order)) + "\n"
    ser.write(msg.encode())

# --- Start with a round ---
current_round = start_new_round()
print("START")

# --- Main loop ---
try:
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if not line:
            continue

        if line == "ROUND:RESET":
            current_round = start_new_round()
            send_order([])
            print("|")
            print(f"[NEW ROUND] — ID = {current_round}")
            continue

        if line.startswith("BUZZ:"):
            try:
                team = int(line.split(":")[1])
                if 1 <= team <= 4:
                    inserted = False
                    try:
                        add_buzz(current_round, team)
                        inserted = True
                    except sqlite3.IntegrityError:
                        print(f"[DUPLICATE] Team {team} already buzzed")
                    order = get_order(current_round)
                    send_order(order)
                    print("|")
                    print(f"| [ROUND {current_round}] ORDER: {order} ({'new buzz' if inserted else 'duplicate'})")
            except ValueError:
                pass
            continue

except KeyboardInterrupt:
    print("\n[EXIT] Interrupted by user.")
finally:
    ser.close()
    con.close()