import streamlit as st
import pandas as pd
import subprocess
import os
import json
import hashlib
import time
from datetime import datetime

st.set_page_config(page_title="Expense Tracker", page_icon="💰", layout="wide")

EXE_PATH = "expense_tracker.exe"
USERS_FILE = "users.json"

def get_user_file(username):
    return f"transactions_{username}.txt"

def load_users():
    if not os.path.exists(USERS_FILE):
        return {}
    with open(USERS_FILE, "r") as f:
        return json.load(f)

def save_users(users):
    with open(USERS_FILE, "w") as f:
        json.dump(users, f)

def hash_password(password):
    return hashlib.sha256(password.encode()).hexdigest()

def login(username, password):
    users = load_users()
    if username in users and users[username] == hash_password(password):
        return True
    return False

def signup(username, password):
    users = load_users()
    if username in users:
        return False
    users[username] = hash_password(password)
    save_users(users)
    return True

def update_password(username, old_password, new_password):
    users = load_users()
    if username not in users:
        return False, "User not found"
    if users[username] != hash_password(old_password):
        return False, "Current password is incorrect"
    users[username] = hash_password(new_password)
    save_users(users)
    return True, "Password updated successfully!"

def admin_delete_user(username):
    users = load_users()
    if username in users:
        del users[username]
        save_users(users)
        user_file = get_user_file(username)
        if os.path.exists(user_file):
            os.remove(user_file)
        return True, f"User '{username}' deleted successfully."
    return False, "User not found."

def admin_change_password(username, new_password):
    users = load_users()
    if username in users:
        users[username] = hash_password(new_password)
        save_users(users)
        return True, f"Password for '{username}' updated successfully."
    return False, "User not found."

if 'logged_in' not in st.session_state:
    st.session_state['logged_in'] = False
if 'username' not in st.session_state:
    st.session_state['username'] = ""

def run_backend(args, username):
    user_file = get_user_file(username)
    try:
        result = subprocess.run([EXE_PATH, user_file] + args, capture_output=True, text=True)
        return result.stdout
    except FileNotFoundError:
        return "Error: Backend executable not found. Please compile main.c first."

def load_data(username):
    user_file = get_user_file(username)
    if not os.path.exists(user_file):
        return pd.DataFrame(columns=["ID", "Day", "Month", "Year", "Amount", "Type", "Category", "Description"])
    
    data = []
    with open(user_file, "r") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 8:
                desc = " ".join(parts[7:])
                data.append({
                    "ID": int(parts[0]),
                    "Day": int(parts[1]),
                    "Month": int(parts[2]),
                    "Year": int(parts[3]),
                    "Amount": float(parts[4]),
                    "Type": parts[5],
                    "Category": parts[6],
                    "Description": desc
                })
    return pd.DataFrame(data)

def load_all_data():
    users = load_users()
    all_data = []
    
    for username in users.keys():
        user_file = get_user_file(username)
        if os.path.exists(user_file):
            with open(user_file, "r") as f:
                for line in f:
                    parts = line.strip().split()
                    if len(parts) >= 8:
                        desc = " ".join(parts[7:])
                        all_data.append({
                            "User": username,
                            "ID": int(parts[0]),
                            "Day": int(parts[1]),
                            "Month": int(parts[2]),
                            "Year": int(parts[3]),
                            "Amount": float(parts[4]),
                            "Type": parts[5],
                            "Category": parts[6],
                            "Description": desc
                        })
    return pd.DataFrame(all_data)

def clean_backend_output(output):
    lines = output.split('\n')
    cleaned = []
    for line in lines:
        line = line.strip()
        if not line: continue
        if "Data loaded successfully" in line: continue
        if "Data saved successfully" in line: continue
        cleaned.append(line)
    return "\n".join(cleaned)

if not st.session_state['logged_in']:
    st.title("🔐 Welcome to Debt Defeaters")
    
    tab1, tab2 = st.tabs(["Login", "Sign Up"])
    
    with tab1:
        st.subheader("Login")
        l_user = st.text_input("Username", key="l_user")
        l_pass = st.text_input("Password", type="password", key="l_pass")
        if st.button("Login"):
            if l_user == "admin" and l_pass == "admin":
                st.session_state['logged_in'] = True
                st.session_state['username'] = "Admin"
                st.session_state['is_admin'] = True
                st.rerun()
            elif login(l_user, l_pass):
                st.session_state['logged_in'] = True
                st.session_state['username'] = l_user
                st.session_state['is_admin'] = False
                st.rerun()
            else:
                st.error("Invalid username or password")
                
    with tab2:
        st.subheader("Sign Up")
        s_user = st.text_input("Username", key="s_user")
        s_pass = st.text_input("Password", type="password", key="s_pass")
        if st.button("Sign Up"):
            if s_user == "admin":
                st.error("Cannot sign up as admin.")
            elif s_user and s_pass:
                if signup(s_user, s_pass):
                    st.success("Account created! Please login.")
                else:
                    st.error("Username already exists.")
            else:
                st.error("Please fill all fields")

else:
    st.sidebar.title(f"👤 {st.session_state['username']}")
    if st.sidebar.button("Logout"):
        st.session_state['logged_in'] = False
        st.session_state['username'] = ""
        st.session_state['is_admin'] = False
        st.rerun()
        
    st.sidebar.divider()
    st.sidebar.title("💰 Debt Defeaters")
    
    if st.session_state.get('is_admin', False):
        st.sidebar.success("Admin Mode Active")
        menu = st.sidebar.radio("Admin Menu", ["Suggestions", "All Transactions", "Settings"])
        
        if menu == "Suggestions":
            st.title("🛡️ Admin Panel - Suggestions")
            output = run_backend(["view_suggestions"], "")
            
            clean_lines = []
            for line in output.split('\n'):
                if line.strip() and ". " in line and not any(x in line for x in ["No existing data", "Starting fresh", "Usage:", "Commands:", "Unknown command"]):
                    clean_lines.append(line)
            
            if not clean_lines or "No suggestions found" in output:
                st.info("No suggestions yet.")
            else:
                for line in clean_lines:
                    try:
                        line_num_str, content = line.split(". ", 1)
                        line_num = int(line_num_str)
                        
                        if ": " in content:
                            s_user, s_text = content.split(": ", 1)
                        else:
                            s_user = "Unknown"
                            s_text = content
                            
                        with st.expander(f"Suggestion from {s_user}"):
                            st.write(s_text)
                            col1, col2 = st.columns([1, 4])
                            
                            if col1.button("Delete", key=f"del_{line_num}"):
                                res = run_backend(["delete_suggestion", str(line_num)], "")
                                if "successfully" in res:
                                    st.toast("Suggestion deleted!", icon="✅")
                                    time.sleep(1)
                                    st.rerun()
                            
                            with col2:
                                with st.popover("Reply"):
                                    reply_text = st.text_input("Reply Message", key=f"rep_{line_num}")
                                    if st.button("Send Reply", key=f"send_{line_num}"):
                                        if reply_text:
                                            res = run_backend(["reply_user", s_user, reply_text], "")
                                            st.toast(f"Reply sent to {s_user}!", icon="✅")
                                        else:
                                            st.error("Enter a message")
                    except Exception as e:
                        pass
            
        elif menu == "All Transactions":
            st.title("📊 All User Transactions")
            df = load_all_data()
            st.dataframe(df, use_container_width=True)

        elif menu == "Settings":
            st.title("⚙️ Admin Settings")
            
            tab1, tab2 = st.tabs(["Change My Password", "Manage Users"])
            
            with tab1:
                st.subheader("Change Admin Password")
                with st.form("admin_pwd_form"):
                    current_pwd = st.text_input("Current Password", type="password")
                    new_pwd = st.text_input("New Password", type="password")
                    confirm_pwd = st.text_input("Confirm New Password", type="password")
                    if st.form_submit_button("Update Password"):
                        if new_pwd == confirm_pwd and len(new_pwd) >= 4:
                            st.warning("Default Admin password cannot be changed in this version.")
                        else:
                            st.error("Invalid input")

            with tab2:
                st.subheader("Manage Users")
                users = load_users()
                user_list = list(users.keys())
                
                if user_list:
                    selected_user = st.selectbox("Select User", user_list)
                    
                    col1, col2 = st.columns(2)
                    
                    with col1:
                        st.write("### Change User Password")
                        new_user_pass = st.text_input(f"New Password for {selected_user}", type="password")
                        if st.button("Update User Password"):
                            if len(new_user_pass) >= 4:
                                success, msg = admin_change_password(selected_user, new_user_pass)
                                if success: st.success(msg)
                                else: st.error(msg)
                            else:
                                st.error("Password too short")
                                
                    with col2:
                        st.write("### Delete User")
                        st.warning(f"Are you sure you want to delete '{selected_user}'? This cannot be undone.")
                        if st.button("Delete User", type="primary"):
                            success, msg = admin_delete_user(selected_user)
                            if success: 
                                st.success(msg)
                                time.sleep(1)
                                st.rerun()
                            else: st.error(msg)
                else:
                    st.info("No users found.")
            
    else:
        menu = st.sidebar.radio("Menu", ["Dashboard", "Add Transaction", "Manage", "Recurring Payments", "Analysis", "Inbox", "Suggestion Box", "Settings"])

        if menu == "Inbox":
            st.title("📬 Inbox")
            output = run_backend(["view_replies", st.session_state['username']], "")
            
            clean_lines = []
            for line in output.split('\n'):
                line_stripped = line.strip()
                if not line_stripped:
                    continue
                if line_stripped in ["No existing data found. Starting fresh."] or \
                   line_stripped.startswith("Usage:") or \
                   line_stripped.startswith("Commands:") or \
                   line_stripped.startswith("Unknown command:"):
                    continue
                clean_lines.append(line)
            
            clean_output = "\n".join(clean_lines)
            
            if "No new messages" in clean_output or not clean_output.strip():
                st.info("No new messages from Admin.")
            else:
                st.success("Messages from Admin:")
                st.text(clean_output)

        if menu == "Dashboard":
            st.title("📊 Financial Dashboard")
            df = load_data(st.session_state['username'])
            
            if not df.empty:
                total_income = df[df["Type"] == "Income"]["Amount"].sum()
                total_expense = df[df["Type"] == "Expense"]["Amount"].sum()
                savings = total_income - total_expense
                
                col1, col2, col3 = st.columns(3)
                col1.metric("Total Income", f"₹{total_income:,.2f}")
                col2.metric("Total Expense", f"₹{total_expense:,.2f}")
                col3.metric("Net Savings", f"₹{savings:,.2f}", delta_color="normal")
                
                st.subheader("Recent Transactions")
                st.dataframe(df.tail(10), use_container_width=True)
            else:
                st.info("No transactions found. Go to 'Add Transaction' to get started!")

        elif menu == "Add Transaction":
            st.title("➕ Add New Transaction")
            
            with st.form("add_form"):
                col2, col3 = st.columns(2)
                amount = col2.number_input("Amount", min_value=0.0, step=0.01)
                
                today = datetime.now()
                day = col3.number_input("Day", 1, 31, today.day)
                
                col4, col5 = st.columns(2)
                month = col4.number_input("Month", 1, 12, today.month)
                year = col5.number_input("Year", 2000, 2100, today.year)
                
                t_type = st.selectbox("Type", ["Income", "Expense"])
                category = st.text_input("Category (e.g., Food, Rent)")
                desc = st.text_input("Description")
                
                submitted = st.form_submit_button("Add Transaction")
                
                if submitted:
                    if category and desc:
                        output = run_backend([
                            "add", str(day), str(month), str(year), 
                            str(amount), t_type, category, desc
                        ], st.session_state['username'])
                        if "successfully" in output.lower():
                            msg = "✅ Transaction added successfully!"
                            if "ID:" in output:
                                try:
                                    new_id = output.split("ID:")[1].strip()
                                    msg = f"✅ Transaction added! (ID: {new_id})"
                                except:
                                    pass
                            st.toast(msg, icon="✅")
                            time.sleep(2)
                            st.rerun()
                        else:
                            st.toast("❌ Failed to add transaction", icon="❌")
                    else:
                        st.toast("Please fill all fields.", icon="⚠️")

        elif menu == "Manage":
            st.title("🛠️ Manage Transactions")
            
            tab1, tab2, tab3 = st.tabs(["Delete", "Sort", "Search"])
            
            with tab1:
                st.subheader("Delete Transaction")
                d_id = st.number_input("Enter ID to Delete", min_value=1, step=1)
                if st.button("Delete"):
                    output = run_backend(["delete", str(d_id)], st.session_state['username'])
                    if "successfully" in output.lower():
                        st.toast(f"✅ Transaction {d_id} deleted successfully!", icon="✅")
                        time.sleep(2)
                        st.rerun()
                    elif "not found" in output.lower():
                        st.toast(f"⚠️ Transaction {d_id} not found", icon="⚠️")
                    else:
                        st.toast("❌ Failed to delete transaction", icon="❌")
                    
            with tab2:
                st.subheader("Sort Transactions")
                col1, col2 = st.columns(2)
                
                if col1.button("Sort by Amount"):
                    output = run_backend(["sort_amount"], st.session_state['username'])
                    st.toast("✅ Transactions sorted by amount!", icon="✅")
                    st.session_state['show_sorted'] = True
                    time.sleep(0.5)
                    st.rerun()
                    
                if col2.button("Sort by Date"):
                    output = run_backend(["sort_date"], st.session_state['username'])
                    st.toast("✅ Transactions sorted by date!", icon="✅")
                    st.session_state['show_sorted'] = True
                    time.sleep(0.5)
                    st.rerun()
                
                if st.session_state.get('show_sorted', False):
                    st.divider()
                    st.write("### Sorted Transactions")
                    df_sorted = load_data(st.session_state['username'])
                    st.dataframe(df_sorted, use_container_width=True)
                    
            with tab3:
                st.subheader("Search Transactions")
                search_type = st.selectbox("Search By", ["Amount", "ID", "Description"])
                
                if search_type == "Amount":
                    search_val = st.number_input("Enter Amount", min_value=0.0, step=0.01)
                    if st.button("Search"):
                        output = run_backend(["search", "amount", str(search_val)], st.session_state['username'])
                        st.text_area("Results", output, height=150)
                        
                elif search_type == "ID":
                    search_val = st.number_input("Enter ID", min_value=1, step=1)
                    if st.button("Search"):
                        output = run_backend(["search", "id", str(search_val)], st.session_state['username'])
                        st.text_area("Results", output, height=150)
                        
                elif search_type == "Description":
                    search_val = st.text_input("Enter Description Keyword")
                    if st.button("Search"):
                        if search_val:
                            output = run_backend(["search", "description", search_val], st.session_state['username'])
                            st.text_area("Results", output, height=150)
                        else:
                            st.warning("Please enter a keyword")

            st.divider()
            st.subheader("Undo Last Action")
            if st.button("Undo Last Operation", type="primary"):
                output = run_backend(["undo"], st.session_state['username'])
                clean_msg = clean_backend_output(output)
                if "Undo:" in clean_msg:
                    st.toast(clean_msg, icon="✅")
                    time.sleep(1)
                    st.rerun()
                elif "Nothing to undo" in clean_msg:
                    st.toast("Nothing to undo.", icon="ℹ️")
                else:
                    st.error("Failed to undo.")

        elif menu == "Recurring Payments":
            st.title("🔄 Recurring Payments")
            
            tab1, tab2, tab3 = st.tabs(["Schedule New", "View Scheduled", "Process Next"])
            
            with tab1:
                st.subheader("Schedule Recurring Payment")
                with st.form("recur_form"):
                    col2, col3 = st.columns(2)
                    amount = col2.number_input("Amount", min_value=0.0, step=0.01)
                    
                    today = datetime.now()
                    day = col3.number_input("Day", 1, 31, today.day)
                    
                    col4, col5 = st.columns(2)
                    month = col4.number_input("Month", 1, 12, today.month)
                    year = col5.number_input("Year", 2000, 2100, today.year)
                    
                    t_type = st.selectbox("Type", ["Income", "Expense"])
                    category = st.text_input("Category (e.g., Rent, Subscription)")
                    desc = st.text_input("Description")
                    
                    if st.form_submit_button("Schedule"):
                        if category and desc:
                            output = run_backend([
                                "recurring", str(day), str(month), str(year), 
                                str(amount), t_type, category, desc
                            ], st.session_state['username'])
                            
                            if "scheduled" in output.lower():
                                st.toast("✅ Recurring payment scheduled!", icon="✅")
                            else:
                                st.error("Failed to schedule payment.")
                        else:
                            st.warning("Please fill all fields.")

            with tab2:
                st.subheader("Scheduled Payments")
                if st.button("Refresh List"):
                    st.rerun()
                    
                output = run_backend(["view_recurring"], st.session_state['username'])
                
                cleaned_output = clean_backend_output(output)
                
                clean_lines = [line for line in cleaned_output.split('\n') if "---" not in line and "Date" not in line and "No upcoming" not in line]
                
                if not clean_lines:
                    st.info("No recurring payments scheduled.")
                else:
                    data = []
                    for line in clean_lines:
                        parts = line.split()
                        if len(parts) >= 4:
                            data.append({
                                "Date": parts[0],
                                "Amount": parts[1],
                                "Category": parts[2],
                                "Description": " ".join(parts[3:])
                            })
                    if data:
                        st.dataframe(pd.DataFrame(data), use_container_width=True)
                    else:
                        st.info("No recurring payments scheduled.")

            with tab3:
                st.subheader("Process Next Payment")
                st.write("Process the next scheduled payment and add it to your transactions.")
                if st.button("Process Next"):
                    output = run_backend(["process_recurring"], st.session_state['username'])
                    clean_msg = clean_backend_output(output)
                    if "Processed" in clean_msg:
                        st.success(clean_msg)
                        time.sleep(2)
                        st.rerun()
                    elif "No recurring payments" in clean_msg:
                        st.info("No payments to process.")
                    else:
                        st.error("Error processing payment.")

        elif menu == "Analysis":
            st.title("📈 Financial Analysis")
            if st.button("Generate Report"):
                output = run_backend(["analysis"], st.session_state['username'])
                st.text(output)
            
            df = load_data(st.session_state['username'])
            if not df.empty:
                st.subheader("Income vs Expense")
                st.bar_chart(df.groupby("Type")["Amount"].sum())
                
                st.subheader("Spending by Category")
                expenses = df[df["Type"] == "Expense"]
                if not expenses.empty:
                    st.bar_chart(expenses.groupby("Category")["Amount"].sum())

        elif menu == "Suggestion Box":
            st.title("💡 Suggestion Box")
            st.write("We value your feedback! Let us know how we can improve.")
            suggestion = st.text_area("Your Suggestion")
            if st.button("Submit Suggestion"):
                if suggestion:
                    output = run_backend(["suggest", st.session_state['username'], suggestion], st.session_state['username'])
                    st.toast("✅ Thank you for your suggestion!", icon="✅")
                else:
                    st.toast("Please enter a suggestion.", icon="⚠️")

        elif menu == "Settings":
            st.title("⚙️ Settings")
            
            st.subheader("Change Password")
            with st.form("password_form"):
                current_pwd = st.text_input("Current Password", type="password")
                new_pwd = st.text_input("New Password", type="password")
                confirm_pwd = st.text_input("Confirm New Password", type="password")
                
                submitted = st.form_submit_button("Update Password")
                
                if submitted:
                    if not current_pwd or not new_pwd or not confirm_pwd:
                        st.error("Please fill all fields")
                    elif new_pwd != confirm_pwd:
                        st.error("New passwords do not match")
                    elif len(new_pwd) < 4:
                        st.error("Password must be at least 4 characters long")
                    else:
                        success, message = update_password(
                            st.session_state['username'],
                            current_pwd,
                            new_pwd
                        )
                        if success:
                            st.toast(message, icon="✅")
                        else:
                            st.toast(message, icon="❌")
