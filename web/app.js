// SPA navigation
const pages = Array.from(document.querySelectorAll('.nav-link'));
pages.forEach(link => link.addEventListener('click', (e) => {
  e.preventDefault();
  const page = link.dataset.page;
  document.querySelectorAll('.nav-link').forEach(a => a.classList.remove('active'));
  link.classList.add('active');
  document.querySelectorAll('.page').forEach(p => p.classList.remove('visible'));
  document.getElementById(page).classList.add('visible');
}));

function fmt(n){ return (Number(n).toFixed(2)); }

async function api(path, opts={}){
  const res = await fetch(path, Object.assign({ headers: { 'Content-Type': 'application/x-www-form-urlencoded' } }, opts));
  const ct = res.headers.get('content-type')||'';
  if (ct.includes('application/json')) return res.json();
  return res.text();
}

// Login/signup/logout
const helloUser = document.getElementById('helloUser');
const loginPanel = document.getElementById('loginPanel');
document.getElementById('loginBtn').onclick = async () => {
  const u = document.getElementById('loginUser').value.trim();
  const p = document.getElementById('loginPass').value.trim();
  const resp = await api('/api/login', { method: 'POST', body: `username=${encodeURIComponent(u)}&password=${encodeURIComponent(p)}` });
  if (resp.ok) { helloUser.textContent = `Hi, ${resp.user}`; loginPanel.style.display='none'; refreshAll(); }
  else alert('Login failed');
};
document.getElementById('signupBtn').onclick = async () => {
  const u = document.getElementById('loginUser').value.trim();
  const p = document.getElementById('loginPass').value.trim();
  const resp = await api('/api/signup', { method:'POST', body:`username=${encodeURIComponent(u)}&password=${encodeURIComponent(p)}` });
  alert(resp.ok ? 'Signup OK, now login' : 'Signup failed');
};
document.getElementById('logoutBtn').onclick = () => { helloUser.textContent='Hi, Guest'; loginPanel.style.display='flex'; };

// Dashboard & Report
async function loadDashboard(){
  const d = await api('/api/dashboard');
  document.getElementById('incomeBox').textContent = fmt(d.income);
  document.getElementById('expenseBox').textContent = fmt(d.totalExpenses);
  document.getElementById('balanceBox').textContent = fmt(d.income - d.totalExpenses);
  const tbody = document.getElementById('byCategoryBody'); tbody.innerHTML='';
  d.byCategory.forEach(r => { const tr=document.createElement('tr'); tr.innerHTML = `<td>${r.category}</td><td>${fmt(r.total)}</td>`; tbody.appendChild(tr); });
  // report mirrors dashboard
  document.getElementById('rIncome').textContent = fmt(d.income);
  document.getElementById('rExpense').textContent = fmt(d.totalExpenses);
  document.getElementById('rBalance').textContent = fmt(d.income - d.totalExpenses);
  const rbody = document.getElementById('reportBody'); rbody.innerHTML='';
  d.byCategory.forEach(r => { const tr=document.createElement('tr'); tr.innerHTML = `<td>${r.category}</td><td>${fmt(r.total)}</td>`; rbody.appendChild(tr); });
}

// Expenses list/add/delete
async function loadExpenses(){
  const cat = document.getElementById('filterCategory').value.trim();
  const desc = document.getElementById('filterDesc').value.trim();
  const q = new URLSearchParams(); if (cat) q.set('category', cat); if (desc) q.set('desc', desc);
  const items = await api(`/api/expenses/list${q.toString()?`?${q.toString()}`:''}`);
  const tbody = document.getElementById('expensesBody'); tbody.innerHTML='';
  items.forEach(x => {
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${x.id}</td><td>${fmt(x.amount)}</td><td>${x.date}</td><td>${x.category}</td><td>${x.description}</td><td><button class="btn btn-danger">Delete</button></td>`;
    tr.querySelector('button').onclick = async () => { await api(`/api/transactions/delete?id=${x.id}`); loadExpenses(); loadDashboard(); };
    tbody.appendChild(tr);
  });
}
document.getElementById('applyFilterBtn').onclick = () => loadExpenses();
document.getElementById('addExpenseBtn').onclick = async () => {
  const amount = document.getElementById('expAmount').value.trim();
  const date = document.getElementById('expDate').value.trim();
  const cat = document.getElementById('expCategory').value.trim();
  const desc = document.getElementById('expDesc').value.trim();
  if (!amount || !date || !cat) { alert('Fill amount, date, category'); return; }
  const body = new URLSearchParams({ amount, date, category: cat, description: desc, currency:'INR' }).toString();
  const resp = await api('/api/expenses/add', { method:'POST', body });
  if (resp.ok) { document.getElementById('expAmount').value=''; document.getElementById('expDate').value=''; document.getElementById('expCategory').value=''; document.getElementById('expDesc').value=''; loadExpenses(); loadDashboard(); }
  else alert('Failed to add');
};

// Budgets
async function loadBudgets(){
  const list = await api('/api/budgets/list');
  const tbody = document.getElementById('budgetsBody'); tbody.innerHTML='';
  list.forEach(b => { const tr = document.createElement('tr'); tr.innerHTML = `<td>${b.category}</td><td>${fmt(b.amount)}</td>`; tbody.appendChild(tr); });
}
document.getElementById('setBudgetBtn').onclick = async () => {
  const category = document.getElementById('budCategory').value.trim();
  const amount = document.getElementById('budAmount').value.trim();
  const body = new URLSearchParams({ category, amount }).toString();
  const resp = await api('/api/budgets/set', { method:'POST', body });
  if (resp.ok) { document.getElementById('budCategory').value=''; document.getElementById('budAmount').value=''; loadBudgets(); }
  else alert('Budget update failed');
};

async function refreshAll(){ await loadDashboard(); await loadExpenses(); await loadBudgets(); }

// Initialize: if demo user exists, try auto-login for convenience
(async function init(){
  try {
    const me = await api('/api/me');
    if (me.loggedIn && me.user) { helloUser.textContent = `Hi, ${me.user}`; loginPanel.style.display='none'; }
    else {
      // attempt auto-login demo/demo if present
      const resp = await api('/api/login', { method:'POST', body:'username=demo&password=demo' });
      if (resp.ok) { helloUser.textContent = `Hi, ${resp.user}`; loginPanel.style.display='none'; }
    }
  } catch {}
  refreshAll();
})();