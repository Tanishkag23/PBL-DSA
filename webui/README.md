# Expense Tracker Web UI

A minimal Flask web UI that reads/writes the same CSVs as the existing C CLI.

## Files
- Uses CSVs in parent folder:
  - `users.csv`
  - `expenses.csv`
  - `incomes.csv`
  - `budgets.csv`

## Setup

1. Create a venv (recommended):
```bash
python3 -m venv .venv
source .venv/bin/activate
```
2. Install deps:
```bash
pip install -r requirements.txt
```
3. Run the app:
```bash
export EXPENSE_APP_SECRET=change-me
python app.py
```
4. Open http://127.0.0.1:5000

## Notes
- Login uses `users.csv` from the CLI app. Create a user via CLI or add a row `username,password`.
- Data written by the web UI is compatible with the CLI.
