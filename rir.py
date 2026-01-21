import time
import os
import requests
import glob
import ctypes # Biblioteka do sprawdzania blokady Windows

# --- KONFIGURACJA ---
LOG_FOLDER = os.path.expandvars(r'%localappdata%\Packages\MSTeams_8wekyb3d8bbwe\LocalCache\Microsoft\MSTeams\Logs')
ESP_URL = "http://10.177.10.155/webhook"

def is_windows_locked():
    """Sprawdza bezpośrednio w systemie, czy stacja robocza jest zablokowana."""
    user32 = ctypes.windll.user32
    # Sprawdza czy pulpit jest zablokowany (0 oznacza odblokowany)
    return user32.GetForegroundWindow() == 0

def get_latest_log_file():
    list_of_files = glob.glob(os.path.join(LOG_FOLDER, 'MSTeams_*.log'))
    if not list_of_files: return None
    return max(list_of_files, key=os.path.getmtime)

def send_to_esp(availability):
    payload = {"presence": {"availability": availability}}
    try:
        requests.post(ESP_URL, json=payload, timeout=1)
        print(f"[{time.strftime('%H:%M:%S')}] Status: {availability}")
    except:
        pass

def main():
    print("=== MONITOR TEAMS + AUTO-BLOKADA SYSTEMU ===")
    
    current_log = get_latest_log_file()
    last_status = None
    last_lock_state = False

    if not current_log:
        print("Błąd: Nie znaleziono logów.")
        return

    with open(current_log, "r", encoding="utf-8", errors="ignore") as f:
        f.seek(0, os.SEEK_END)
        
        while True:
            # 1. Sprawdź blokadę Windows (Win + L) niezależnie od logów
            locked = is_windows_locked()
            if locked != last_lock_state:
                if locked:
                    send_to_esp("Away")
                    print(">>> System zablokowany (Win+L)")
                last_lock_state = locked
                time.sleep(1) # Chwila przerwy po zmianie stanu

            # Jeśli zablokowany, nie czytaj logów (priorytet dla Away)
            if locked:
                time.sleep(1)
                continue

            # 2. Czytaj logi Teams, jeśli komputer jest odblokowany
            line = f.readline()
            if not line:
                # Sprawdź czy nie powstał nowy plik logu
                newest = get_latest_log_file()
                if newest and newest != current_log:
                    current_log = newest
                    f = open(current_log, "r", encoding="utf-8", errors="ignore")
                    f.seek(0, os.SEEK_END)
                time.sleep(0.5)
                continue
            
            l = line.lower()
            new_status = None

            # Wykrywanie spotkania lub powrotu do aktywności
            if "sendmeetingupdate" in l or "is_in_meeting=true" in l:
                new_status = "Busy"
            elif "is_in_meeting=false" in l or "is_active=true" in l:
                new_status = "Available"
            elif "system_is_locked: true" in l or "is_active=false" in l:
                new_status = "Away"

            if new_status and new_status != last_status:
                send_status = new_status
                send_to_esp(send_status)
                last_status = new_status

if __name__ == "__main__":
    main()