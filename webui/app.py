from flask import Flask, render_template, request, redirect, url_for, session, flash
import csv
import os
from pathlib import Path

# Paths to CSV files (shared with C CLI)
BASE_DIR = Path(__file__).resolve().parent.parent
USERS_FILE = BASE_DIR / 'users.csv'
EXPENSES_FILE = BASE_DIR / 'expenses.csv'
INCOMES_FILE = BASE_DIR / 'incomes.csv'
BUDGETS_FILE = BASE_DIR / 'budgets.csv'

app = Flask(__name__)
app.secret_key = os.environ.get('EXPENSE_APP_SECRET', 'dev-secret')

# ---- Helpers ----

def read_users():
    users = {}
    if USERS_FILE.exists():
        with open(USERS_FILE, newline='') as f:
            for row in csv.reader(f):
                if len(row) >= 2:
                    users[row[0]] = row[1]
    return users


def read_expenses(username):
    items = []
    if EXPENSES_FILE.exists():
        with open(EXPENSES_FILE, newline='') as f:
            for row in csv.reader(f):
                if len(row) >= 6 and row[0] == username:
                    try:
                        items.append({
                            'id': int(row[1]),
                            'amount': float(row[2]),
                            'date': int(row[3]),
                            'category': row[4],
                            'description': row[5]
                        })
                    except Exception:
                        continue
    return items


def write_expenses(username, items):
    # Keep other users' lines; rewrite current user's lines
    other_lines = []
    if EXPENSES_FILE.exists():
        with open(EXPENSES_FILE, newline='') as f:
            for line in f:
                parts = line.strip().split(',')
                if parts and parts[0] != username:
                    other_lines.append(line)
    with open(EXPENSES_FILE, 'w', newline='') as f:
        for line in other_lines:
            f.write(line)
        w = csv.writer(f)
        for it in items:
            w.writerow([username, it['id'], f"{it['amount']:.2f}", it['date'], it['category'], it['description']])


def read_income(username):
    if INCOMES_FILE.exists():
        with open(INCOMES_FILE, newline='') as f:
            for row in csv.reader(f):
                if len(row) >= 2 and row[0] == username:
                    try:
                        return float(row[1])
                    except Exception:
                        return 0.0
    return 0.0


def write_income(username, amount):
    other_lines = []
    if INCOMES_FILE.exists():
        with open(INCOMES_FILE, newline='') as f:
            for line in f:
                parts = line.strip().split(',')
                if parts and parts[0] != username:
                    other_lines.append(line)
    with open(INCOMES_FILE, 'w', newline='') as f:
        for line in other_lines:
            f.write(line)
        w = csv.writer(f)
        w.writerow([username, f"{amount:.2f}"])


def read_budgets(username):
    data = {}
    if BUDGETS_FILE.exists():
        with open(BUDGETS_FILE, newline='') as f:
            for row in csv.reader(f):
                if len(row) >= 3 and row[0] == username:
                    try:
                        data[row[1]] = float(row[2])
                    except Exception:
                        continue
    return data


def write_budgets(username, budgets_map):
    other_lines = []
    if BUDGETS_FILE.exists():
        with open(BUDGETS_FILE, newline='') as f:
            for line in f:
                parts = line.strip().split(',')
                if parts and parts[0] != username:
                    other_lines.append(line)
    with open(BUDGETS_FILE, 'w', newline='') as f:
        for line in other_lines:
            f.write(line)
        w = csv.writer(f)
        for cat, amt in budgets_map.items():
            w.writerow([username, cat, f"{amt:.2f}"])


def require_login():
    if 'user' not in session:
        return redirect(url_for('login'))
    return None


# ---- Routes ----
@app.route('/', methods=['GET'])
def root():
    if 'user' in session:
        return redirect(url_for('dashboard'))
    return redirect(url_for('login'))


@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        user = request.form.get('username', '').strip()
        password = request.form.get('password', '')
        users = read_users()
        if user in users and users[user] == password:
            session['user'] = user
            return redirect(url_for('dashboard'))
        flash('Invalid username or password', 'error')
    return render_template('login.html')


@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login'))


@app.route('/dashboard', methods=['GET', 'POST'])
def dashboard():
    r = require_login()
    if r: return r
    user = session['user']
    if request.method == 'POST':
        income_str = request.form.get('income', '0')
        try:
            income = float(income_str)
        except Exception:
            income = 0.0
        write_income(user, max(income, 0.0))
        flash('Income updated', 'ok')
        return redirect(url_for('dashboard'))

    items = read_expenses(user)
    income = read_income(user)
    total = sum(it['amount'] for it in items)
    balance = income - total

    # aggregate by category
    cat_totals = {}
    for it in items:
        cat_totals[it['category']] = cat_totals.get(it['category'], 0.0) + it['amount']

    return render_template('dashboard.html', items=items, income=income, total=total, balance=balance, cat_totals=cat_totals)


@app.route('/expenses', methods=['GET', 'POST'])
def expenses():
    r = require_login()
    if r: return r
    user = session['user']
    items = read_expenses(user)

    if request.method == 'POST':
        # add new expense
        try:
            amount = float(request.form.get('amount', '0'))
        except Exception:
            amount = 0.0
        date = int(request.form.get('date', '0') or 0)
        category = request.form.get('category', '').strip()
        desc = request.form.get('description', '').strip()
        next_id = 1 + max((it['id'] for it in items), default=0)
        items.append({'id': next_id, 'amount': amount, 'date': date, 'category': category, 'description': desc})
        write_expenses(user, items)
        flash('Expense added', 'ok')
        return redirect(url_for('expenses'))

    # Optional filters
    q_cat = request.args.get('category', '').strip()
    q_text = request.args.get('q', '').strip()
    filtered = items
    if q_cat:
        filtered = [it for it in filtered if it['category'] == q_cat]
    if q_text:
        filtered = [it for it in filtered if q_text.lower() in it['description'].lower()]

    return render_template('expenses.html', items=filtered, q_cat=q_cat, q_text=q_text)


@app.route('/expenses/delete/<int:item_id>', methods=['POST'])
def delete_expense(item_id):
    r = require_login()
    if r: return r
    user = session['user']
    items = read_expenses(user)
    items = [it for it in items if it['id'] != item_id]
    write_expenses(user, items)
    flash('Expense deleted', 'ok')
    return redirect(url_for('expenses'))


@app.route('/budgets', methods=['GET', 'POST'])
def budgets():
    r = require_login()
    if r: return r
    user = session['user']
    budgets_map = read_budgets(user)

    if request.method == 'POST':
        cat = request.form.get('category', '').strip()
        amt_str = request.form.get('amount', '0')
        try:
            amt = float(amt_str)
        except Exception:
            amt = 0.0
        if cat:
            if amt <= 0:
                budgets_map.pop(cat, None)
            else:
                budgets_map[cat] = amt
            write_budgets(user, budgets_map)
            flash('Budget updated', 'ok')
        return redirect(url_for('budgets'))

    # compute usage per category
    items = read_expenses(user)
    cat_totals = {}
    for it in items:
        cat_totals[it['category']] = cat_totals.get(it['category'], 0.0) + it['amount']

    alerts = []
    for cat, b in budgets_map.items():
        t = cat_totals.get(cat, 0.0)
        if t >= b:
            alerts.append((cat, t, b, 'exceeded'))
        elif t >= 0.8*b:
            alerts.append((cat, t, b, 'warning'))

    return render_template('budgets.html', budgets=budgets_map, alerts=alerts)


@app.route('/report')
def report():
    r = require_login()
    if r: return r
    user = session['user']
    items = read_expenses(user)
    income = read_income(user)
    total = sum(it['amount'] for it in items)
    balance = income - total

    cat_totals = {}
    for it in items:
        cat_totals[it['category']] = cat_totals.get(it['category'], 0.0) + it['amount']

    return render_template('report.html', income=income, total=total, balance=balance, cat_totals=cat_totals)


if __name__ == '__main__':
    port = int(os.environ.get('PORT', '5000'))
    app.run(host='127.0.0.1', port=port, debug=True)
