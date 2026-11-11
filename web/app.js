/* =============================================================================
 * app.js - web user interface for the Car Rental Management System.
 * -----------------------------------------------------------------------------
 * This file contains NO business rules. Every operation is a call into the
 * C++ class WebApp (web/wasm/WebBindings.cpp), which uses the same C++
 * RentalAgency, Vehicle, Money, Validation, RentalStatistics, UserDirectory
 * and DataFileFormat classes as the desktop program, compiled to WebAssembly.
 * This file only draws the screen and moves the data "bundle" to and from:
 *   - the shared online database (/api/data on the same site), if it is set up,
 *   - otherwise this browser's local storage.
 * ========================================================================== */
'use strict';

(function () {
  const STORAGE_BUNDLE = 'crms.bundle';
  const OLD_KEYS = ['crms.vehicles.txt', 'crms.revenue.txt', 'crms.history.txt'];
  const TIMES = '×';

  let engine = null;          // the compiled C++ module
  let app = null;             // C++ WebApp instance
  let mode = 'local';         // 'local' (this browser) or 'online' (shared database)
  let remoteVersion = '0';    // version of the shared data we loaded (optimistic concurrency)
  let credentials = null;     // { username, password } for the shared database
  let session = { loggedIn: false, can: {} };
  let lastAction = 'Ready.';
  let wired = false;

  const $ = (id) => document.getElementById(id);

  // ---------------------------------------------------------------- storage --
  function storageGet(key) {
    try { return window.localStorage.getItem(key); } catch (error) { return null; }
  }
  function storageSet(key, value) {
    try { window.localStorage.setItem(key, value); return true; } catch (error) { return false; }
  }
  function storageRemove(key) {
    try { window.localStorage.removeItem(key); } catch (error) { /* ignore */ }
  }

  async function fetchText(path) {
    const response = await fetch(path, { cache: 'no-store' });
    if (!response.ok) throw new Error('Could not download ' + path);
    return response.text();
  }

  // Builds a data bundle (the format of the C++ class DataFileFormat) from separate file texts.
  function makeBundle(vehicles, revenue, history, users) {
    const section = (name, text) => '--- ' + name + '\n' + (text || '').replace(/\r/g, '').replace(/\n*$/, '\n');
    return 'CRMS-BUNDLE 1\n' + section('vehicles.txt', vehicles) + section('revenue.txt', revenue) +
      section('history.txt', history) + section('users.txt', users);
  }

  async function sampleBundle(vehiclesFile = 'data/vehicles.txt') {
    const [vehicles, revenue, history] = await Promise.all([
      fetchText(vehiclesFile), fetchText('data/revenue.txt'), fetchText('data/history.txt')]);
    return makeBundle(vehicles, revenue, history, ''); // no users -> the C++ code creates the default accounts
  }

  // Data saved in this browser (also converts the format of older versions).
  async function localBundle() {
    const saved = storageGet(STORAGE_BUNDLE);
    if (saved !== null) return saved;
    const old = OLD_KEYS.map(storageGet);
    if (old[0] !== null) return makeBundle(old[0], old[1], old[2], '');
    return sampleBundle();
  }

  // ------------------------------------------------------- shared database --
  async function detectMode() {
    try {
      const response = await fetch('api/status', { cache: 'no-store' });
      if (!response.ok) return 'local';
      const status = await response.json();
      return status.configured ? 'online' : 'local';
    } catch (error) {
      return 'local';
    }
  }

  function authHeader(user) {
    const bytes = new TextEncoder().encode(user.username + ':' + user.password);
    let binary = '';
    bytes.forEach((byte) => { binary += String.fromCharCode(byte); });
    return 'Basic ' + btoa(binary);
  }

  async function readError(response) {
    try { const json = await response.json(); return json.error || ('Error ' + response.status); } catch (error) { return 'Error ' + response.status; }
  }

  // GET /api/data -> { bundle, version }. Throws an Error with a user-friendly message.
  async function downloadShared(user) {
    const response = await fetch('api/data', { headers: { Authorization: authHeader(user) }, cache: 'no-store' });
    if (response.status === 401) throw new Error('Incorrect user name or password.');
    if (!response.ok) throw new Error('The shared online database is not available: ' + await readError(response));
    return { bundle: await response.text(), version: response.headers.get('X-CRMS-Version') || '0' };
  }

  // PUT /api/data. Returns { ok, message }.
  async function uploadShared(bundle) {
    const response = await fetch('api/data', {
      method: 'PUT',
      headers: { Authorization: authHeader(credentials), 'Content-Type': 'text/plain; charset=utf-8', 'X-CRMS-Version': remoteVersion },
      body: bundle,
    });
    if (response.ok) {
      remoteVersion = response.headers.get('X-CRMS-Version') || String(Number(remoteVersion) + 1);
      return { ok: true };
    }
    if (response.status === 409) {
      return { ok: false, message: 'The shared data was changed by another user after you loaded it. Your changes were NOT saved. Use Reload to get the latest data, then repeat your changes.' };
    }
    return { ok: false, message: await readError(response) };
  }

  // Calls a C++ method that returns JSON and parses the result.
  function call(method, ...args) {
    return JSON.parse(app[method](...args));
  }

  // ------------------------------------------------------------- feedback --
  function countText(count, singular, plural) {
    return count + ' ' + (count === 1 ? singular : plural);
  }

  function setLastAction(text) {
    lastAction = String(text).replace(/\s+/g, ' ');
    $('sbAction').textContent = lastAction;
  }

  let toastTimer = null;
  function showToast(text) {
    const toast = $('toast');
    toast.textContent = text;
    toast.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { toast.hidden = true; }, 3200);
  }

  function success(text) {
    setLastAction(text);
    showToast(text);
  }

  // ------------------------------------------------------------------ table --
  // Sortable, selectable table. A cell is { text, sort } and may also have
  // badge ('ok' | 'rent'), bar (0-100) or strong (true).
  class ReportTable {
    constructor(tableId, options = {}) {
      this.table = $(tableId);
      this.body = this.table.tBodies[0];
      this.columns = Array.from(this.table.tHead.rows[0].cells);
      this.rows = [];
      this.selectedKey = null;
      this.sortColumn = -1;
      this.ascending = true;
      this.selectable = options.selectable !== false;
      this.onSelect = options.onSelect || (() => {});
      this.onActivate = options.onActivate || null;
      this.emptyText = options.emptyText || 'Nothing to show.';

      this.table.tabIndex = 0;
      this.columns.forEach((th, index) => {
        th.tabIndex = 0;
        th.addEventListener('click', () => this.sortBy(index));
        th.addEventListener('keydown', (event) => {
          if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); this.sortBy(index); }
        });
      });
      this.body.addEventListener('click', (event) => {
        const tr = event.target.closest('tr[data-key]');
        if (tr && this.selectable) this.select(tr.dataset.key);
      });
      this.body.addEventListener('dblclick', (event) => {
        const tr = event.target.closest('tr[data-key]');
        if (tr && this.onActivate) { this.select(tr.dataset.key); this.onActivate(tr.dataset.key); }
      });
      this.table.addEventListener('keydown', (event) => this.handleKey(event));
    }

    setRows(rows) {
      this.rows = rows;
      if (!rows.some((row) => row.key === this.selectedKey)) this.selectedKey = null;
      this.render();
    }

    sortBy(index) {
      if (this.sortColumn === index) this.ascending = !this.ascending;
      else { this.sortColumn = index; this.ascending = true; }
      this.render();
    }

    sortedRows() {
      if (this.sortColumn < 0) return this.rows;
      const column = this.sortColumn;
      const direction = this.ascending ? 1 : -1;
      return this.rows.slice().sort((a, b) => {
        const x = a.cells[column].sort;
        const y = b.cells[column].sort;
        const result = typeof x === 'number' ? x - y : String(x).localeCompare(String(y), undefined, { sensitivity: 'base' });
        return result * direction;
      });
    }

    renderCell(cell, index) {
      const td = document.createElement('td');
      if (this.columns[index].classList.contains('num')) td.className = 'num';
      if (cell.strong) td.classList.add('strong');
      if (cell.badge) {
        const badge = document.createElement('span');
        badge.className = 'badge badge-' + cell.badge;
        badge.textContent = cell.text;
        td.appendChild(badge);
      } else if (typeof cell.bar === 'number') {
        const bar = document.createElement('div');
        bar.className = 'bar';
        const fill = document.createElement('i');
        fill.style.width = Math.max(2, Math.round(cell.bar)) + '%';
        const label = document.createElement('span');
        label.textContent = cell.text;
        bar.append(fill, label);
        td.appendChild(bar);
      } else {
        td.textContent = cell.text;
      }
      return td;
    }

    render() {
      this.columns.forEach((th, index) => {
        th.removeAttribute('aria-sort');
        th.classList.remove('sort-asc', 'sort-desc');
        if (index === this.sortColumn) {
          th.setAttribute('aria-sort', this.ascending ? 'ascending' : 'descending');
          th.classList.add(this.ascending ? 'sort-asc' : 'sort-desc');
        }
      });
      this.body.replaceChildren();
      const rows = this.sortedRows();
      if (rows.length === 0) {
        const tr = document.createElement('tr');
        tr.className = 'empty';
        const td = document.createElement('td');
        td.colSpan = this.columns.length;
        td.textContent = this.emptyText;
        tr.appendChild(td);
        this.body.appendChild(tr);
        return;
      }
      for (const row of rows) {
        const tr = document.createElement('tr');
        tr.dataset.key = row.key;
        if (row.className) tr.className = row.className;
        if (row.key === this.selectedKey) tr.classList.add('selected');
        tr.setAttribute('aria-selected', row.key === this.selectedKey ? 'true' : 'false');
        row.cells.forEach((cell, index) => tr.appendChild(this.renderCell(cell, index)));
        this.body.appendChild(tr);
      }
    }

    select(key) {
      this.selectedKey = key;
      for (const tr of this.body.rows) {
        const selected = tr.dataset.key === key;
        tr.classList.toggle('selected', selected);
        tr.setAttribute('aria-selected', selected ? 'true' : 'false');
        if (selected) tr.scrollIntoView({ block: 'nearest' });
      }
      this.onSelect(key);
    }

    selectedRow() {
      return this.rows.find((row) => row.key === this.selectedKey) || null;
    }

    handleKey(event) {
      if (event.target !== this.table || !this.selectable) return;
      const rows = this.sortedRows();
      if (rows.length === 0) return;
      const index = rows.findIndex((row) => row.key === this.selectedKey);
      if (event.key === 'ArrowDown') {
        event.preventDefault();
        this.select(rows[Math.min(rows.length - 1, index + 1)].key);
      } else if (event.key === 'ArrowUp') {
        event.preventDefault();
        this.select(rows[Math.max(0, index - 1)].key);
      } else if (event.key === 'Enter' && this.selectedKey && this.onActivate) {
        event.preventDefault();
        this.onActivate(this.selectedKey);
      }
    }
  }

  // ---------------------------------------------------------------- dialogs --
  function showMessage(title, text) {
    $('messageTitle').textContent = title;
    $('messageText').textContent = text;
    $('messageDialog').showModal();
  }

  function confirmAction(title, text, okLabel) {
    return new Promise((resolve) => {
      const dialog = $('confirmDialog');
      $('confirmTitle').textContent = title;
      $('confirmText').textContent = text;
      $('confirmOk').textContent = okLabel || 'OK';
      dialog.returnValue = 'cancel';
      dialog.addEventListener('close', () => resolve(dialog.returnValue === 'ok'), { once: true });
      dialog.showModal();
      $('confirmOk').focus();
    });
  }

  // Shows a C++ error inside a form: message box at the top, focus on the bad field
  // (same behaviour as ModalDialog::handleOk() in the desktop program).
  function showFormError(form, result) {
    const errorBox = form.querySelector('.dlg-error');
    errorBox.textContent = result.message;
    errorBox.hidden = false;
    form.querySelectorAll('.invalid').forEach((input) => input.classList.remove('invalid'));
    const field = result.field ? form.elements[result.field] : null;
    if (field && !field.readOnly) {
      field.classList.add('invalid');
      field.focus();
      if (typeof field.select === 'function') field.select();
    }
  }

  function clearFormError(form) {
    const errorBox = form.querySelector('.dlg-error');
    if (errorBox) { errorBox.hidden = true; errorBox.textContent = ''; }
    form.querySelectorAll('.invalid').forEach((input) => input.classList.remove('invalid'));
  }

  function setupDialog(dialog, onOk) {
    const form = dialog.querySelector('form');
    form.addEventListener('submit', (event) => {
      event.preventDefault();
      const result = onOk(form);
      if (result.ok) {
        clearFormError(form);
        dialog.close('ok');
        return;
      }
      showFormError(form, result);
    });
    dialog.querySelectorAll('[data-cancel]').forEach((button) =>
      button.addEventListener('click', () => dialog.close('cancel')));
  }

  function prepareDialog(dialog) {
    clearFormError(dialog.querySelector('form'));
  }

  function fillDetails(list, pairs) {
    list.replaceChildren();
    for (const [label, value] of pairs) {
      const dt = document.createElement('dt');
      dt.textContent = label;
      const dd = document.createElement('dd');
      dd.textContent = value;
      list.append(dt, dd);
    }
  }

  function reject(message) {
    setLastAction('Rejected: ' + message);
    showMessage('Car Rental Management System', message);
  }

  // ----------------------------------------------------------------- tables --
  const fleetTable = new ReportTable('fleetTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => editVehicle(key),
    emptyText: 'No vehicles match. Change the search or the filter, or add a vehicle.',
  });
  const searchTable = new ReportTable('searchTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => rentVehicle(key),
    emptyText: 'No available vehicle matches this model.',
  });
  const rentalsTable = new ReportTable('rentalsTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => returnVehicle(key),
    emptyText: 'No vehicles are rented at the moment.',
  });
  const historyTable = new ReportTable('historyTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => showHistoryReceipt(key),
    emptyText: 'No completed rentals yet. Returned vehicles are archived here.',
  });
  const modelTable = new ReportTable('modelTable', { selectable: false, emptyText: 'No completed rentals yet.' });
  const topTable = new ReportTable('topTable', { selectable: false, emptyText: 'No completed rentals yet.' });
  const usersTable = new ReportTable('usersTable', { onSelect: () => updateButtons(), emptyText: 'No accounts.' });

  let current = { vehicles: [], revenue: '0.00', total: 0, available: 0, rented: 0, modified: false };
  let historyRecords = [];
  let fleetFilter = 'all';

  // ---------------------------------------------------------------- refresh --
  function refreshAll() {
    if (!session.loggedIn) return;
    current = call('state');
    const statsResult = call('statistics'); // C++ RentalStatistics (Managers only)
    const stats = statsResult.ok ? statsResult : null;
    const byPlate = new Map(current.vehicles.map((vehicle) => [vehicle.plate, vehicle]));

    // Fleet (RentalAgency::searchFleet)
    const fleetQuery = $('fleetSearch').value.trim();
    const fleetPlates = call('fleet', fleetQuery, fleetFilter).plates;
    fleetTable.setRows(fleetPlates.map((plate) => byPlate.get(plate)).map((vehicle) => {
      const available = vehicle.status === 'Available';
      return {
        key: vehicle.plate,
        className: available ? 'row-ok' : 'row-rent',
        available,
        cells: [
          { text: vehicle.plate, sort: vehicle.plate, strong: true },
          { text: vehicle.model, sort: vehicle.model },
          { text: vehicle.rate, sort: vehicle.rateCents },
          { text: vehicle.status, sort: vehicle.status, badge: available ? 'ok' : 'rent' },
        ],
      };
    }));
    $('fleetCount').textContent = 'Showing ' + fleetPlates.length + ' of ' + countText(current.total, 'vehicle', 'vehicles') + '.';

    // Search & Rent (RentalAgency::searchAvailableByModel)
    const query = $('searchInput').value.trim();
    const plates = call('search', query).plates;
    searchTable.setRows(plates.map((plate) => byPlate.get(plate)).map((vehicle) => ({
      key: vehicle.plate,
      cells: [
        { text: vehicle.plate, sort: vehicle.plate, strong: true },
        { text: vehicle.model, sort: vehicle.model },
        { text: vehicle.rate, sort: vehicle.rateCents },
      ],
    })));
    $('searchCount').textContent = query === ''
      ? countText(plates.length, 'available vehicle', 'available vehicles') + '. Type to filter by model.'
      : countText(plates.length, 'available vehicle matches', 'available vehicles match') + ' "' + query + '".';

    // Rentals & Returns
    const rented = current.vehicles.filter((vehicle) => vehicle.status === 'Rented');
    rentalsTable.setRows(rented.map((vehicle) => ({
      key: vehicle.plate,
      cells: [
        { text: vehicle.plate, sort: vehicle.plate, strong: true },
        { text: vehicle.model, sort: vehicle.model },
        { text: vehicle.renter, sort: vehicle.renter },
        { text: vehicle.phone, sort: vehicle.phone },
        { text: vehicle.startDate || '-', sort: vehicle.startDate || '' },
        { text: vehicle.agreed, sort: vehicle.agreedCents },
      ],
    })));
    $('revenueLabel').textContent = current.revenue;
    $('dailyIncomeLabel').textContent = stats ? stats.activeDailyIncome : '–';

    // Rental History (RentalAgency::searchHistory)
    const historyQuery = $('historySearch').value.trim();
    const history = call('history', historyQuery);
    historyRecords = history.records;
    historyTable.setRows(historyRecords.map((record) => ({
      key: String(record.number),
      cells: [
        { text: String(record.number), sort: record.number },
        { text: record.returnedOn, sort: record.returnedOn },
        { text: record.plate, sort: record.plate, strong: true },
        { text: record.model, sort: record.model },
        { text: record.renter, sort: record.renter },
        { text: record.phone, sort: record.phone },
        { text: record.rate, sort: record.rateCents },
        { text: String(record.days), sort: record.days },
        { text: record.total, sort: record.totalCents, strong: true },
      ],
    })));
    $('historySummary').textContent = (historyQuery === ''
      ? countText(history.count, 'rental', 'rentals')
      : countText(history.count, 'match', 'matches')) + ' · ' + history.total;

    refreshDashboard(stats);
    if (stats) refreshReports(stats);
    if (session.can.manageUsers) refreshUsers();

    // Navigation counters, status bar, unsaved state
    $('navFleetCount').textContent = String(current.total);
    $('navAvailableCount').textContent = String(current.available);
    $('navRentedCount').textContent = String(current.rented);
    $('sbTotal').textContent = 'Total vehicles: ' + current.total;
    $('sbAvail').textContent = 'Available: ' + current.available;
    $('sbRented').textContent = 'Rented: ' + current.rented;
    $('sbRevenue').textContent = 'Revenue: ' + current.revenue;
    $('sbAction').textContent = lastAction;
    $('unsavedBadge').hidden = !current.modified;
    document.title = (current.modified ? '* ' : '') + 'Car Rental Management System';
    updateButtons();
  }

  function refreshDashboard(stats) {
    const utilization = current.total > 0 ? Math.round((current.rented * 100) / current.total) : 0;
    $('kTotal').textContent = String(current.total);
    $('kTotalNote').textContent = 'in the fleet';
    $('kAvailable').textContent = String(current.available);
    $('kRented').textContent = String(current.rented);
    $('kUtilization').textContent = (stats ? stats.utilization : utilization) + '% of the fleet';
    $('kRevenue').textContent = current.revenue;
    const all = historyRecordsAll();
    $('kRevenueNote').textContent = 'from ' + countText(all.length, 'completed rental', 'completed rentals');
    const percent = stats ? stats.utilization : utilization;
    $('utilFill').style.width = percent + '%';
    $('utilText').textContent = percent + '% rented' + (stats ? ' · active rentals earn ' + stats.activeDailyIncome + ' per day' : '');
    $('utilMeter').setAttribute('aria-label', percent + ' percent of the fleet is rented');

    const list = $('recentList');
    list.replaceChildren();
    const recent = all.slice(-5).reverse();
    if (recent.length === 0) {
      const li = document.createElement('li');
      li.className = 'empty';
      li.textContent = 'No returns yet.';
      list.appendChild(li);
    }
    for (const record of recent) {
      const li = document.createElement('li');
      const avatar = document.createElement('span');
      avatar.className = 'avatar';
      avatar.textContent = record.renter.split(/\s+/).map((part) => part[0] || '').join('').slice(0, 2).toUpperCase();
      const who = document.createElement('span');
      who.className = 'who';
      const name = document.createElement('strong');
      name.textContent = record.renter;
      const detail = document.createElement('small');
      detail.textContent = record.plate + ' · ' + record.model + ' · ' + countText(record.days, 'day', 'days') + ' · ' + record.returnedOn;
      who.append(name, detail);
      const amount = document.createElement('span');
      amount.className = 'amount';
      amount.textContent = record.total;
      li.append(avatar, who, amount);
      li.title = 'Receipt no. ' + record.number;
      li.addEventListener('dblclick', () => showReceipt(record, true));
      list.appendChild(li);
    }
  }

  function historyRecordsAll() {
    return $('historySearch').value.trim() === '' ? historyRecords : call('history', '').records;
  }

  function refreshReports(stats) {
    $('rCompleted').textContent = String(stats.completedRentals);
    $('rTotal').textContent = stats.totalCharged;
    $('rAverage').textContent = stats.averageCharge;
    $('rDays').textContent = stats.averageDays;

    const totalCents = stats.byModel.reduce((sum, model) => sum + model.revenueCents, 0);
    modelTable.setRows(stats.byModel.map((model) => {
      const share = totalCents > 0 ? (model.revenueCents * 100) / totalCents : 0;
      return {
        key: model.model,
        cells: [
          { text: model.model, sort: model.model, strong: true },
          { text: String(model.rentals), sort: model.rentals },
          { text: String(model.days), sort: model.days },
          { text: model.revenue, sort: model.revenueCents },
          { text: Math.round(share) + '%', sort: share, bar: share },
        ],
      };
    }));
    topTable.setRows(stats.topVehicles.map((vehicle, index) => ({
      key: vehicle.plate,
      cells: [
        { text: String(index + 1), sort: index + 1 },
        { text: vehicle.plate, sort: vehicle.plate, strong: true },
        { text: vehicle.model, sort: vehicle.model },
        { text: String(vehicle.rentals), sort: vehicle.rentals },
        { text: vehicle.revenue, sort: vehicle.revenueCents },
      ],
    })));
  }

  function refreshUsers() {
    const result = call('users');
    if (!result.ok) return;
    usersTable.setRows(result.users.map((user) => ({
      key: user.username,
      cells: [
        { text: user.username + (user.username === session.username ? '  (you)' : ''), sort: user.username, strong: true },
        { text: user.role, sort: user.role, badge: user.role === 'Manager' ? 'ok' : 'rent' },
      ],
    })));
  }

  function updateButtons() {
    const can = session.can || {};
    const fleetRow = fleetTable.selectedRow();
    $('addBtn').disabled = !can.manageFleet;
    $('qaAdd').disabled = !can.manageFleet;
    $('editBtn').disabled = !(can.manageFleet && fleetRow && fleetRow.available);
    $('removeBtn').disabled = !(can.manageFleet && fleetRow && fleetRow.available);
    $('rentBtn').disabled = !(can.rentAndReturn && searchTable.selectedRow());
    $('returnBtn').disabled = !(can.rentAndReturn && rentalsTable.selectedRow());
    $('historyReceiptBtn').disabled = !historyTable.selectedRow();
    const userRow = usersTable.selectedRow();
    $('removeUserBtn').disabled = !userRow || userRow.key === session.username;
    $('resetPasswordBtn').disabled = !userRow;
  }

  // Shows or hides everything marked data-permission="..." for the logged-in role.
  function applyPermissions() {
    const can = session.can || {};
    document.querySelectorAll('[data-permission]').forEach((element) => {
      element.hidden = !can[element.dataset.permission];
    });
    $('userName').textContent = session.username || '-';
    $('userRole').textContent = session.role || '-';
    $('userAvatar').textContent = (session.username || '?').slice(0, 2).toUpperCase();
    const page = currentPage();
    const navItem = document.querySelector('.nav-item[data-page="' + page + '"]');
    if (navItem && navItem.hidden) selectPage('dashboard', { focus: false });
  }

  // -------------------------------------------------------------- use cases --
  let editingPlate = null; // null = add mode

  function openVehicleDialog(vehicle) {
    if (!session.can.manageFleet) { reject('Access denied: only a Manager may add, edit or remove vehicles.'); return; }
    const dialog = $('vehicleDialog');
    const form = dialog.querySelector('form');
    prepareDialog(dialog);
    editingPlate = vehicle ? vehicle.plate : null;
    $('vehicleDialogTitle').textContent = vehicle ? 'Edit Vehicle' : 'Add Vehicle';
    form.elements.plate.value = vehicle ? vehicle.plate : '';
    form.elements.plate.readOnly = Boolean(vehicle);
    $('plateHint').textContent = vehicle ? 'The license plate cannot be changed.' : "Letters A-Z, digits 0-9 and '-' only, e.g. ABC-123.";
    form.elements.model.value = vehicle ? vehicle.model : '';
    form.elements.rate.value = vehicle ? vehicle.ratePlain : '';
    dialog.showModal();
    (vehicle ? form.elements.model : form.elements.plate).focus();
  }

  setupDialog($('vehicleDialog'), (form) => {
    const model = form.elements.model.value;
    const rate = form.elements.rate.value;
    if (editingPlate === null) {
      const result = call('addVehicle', form.elements.plate.value, model, rate);
      if (result.ok) {
        success('Added vehicle ' + result.plate + '.');
        selectPage('fleet');
        refreshAll();
        fleetTable.select(result.plate);
      }
      return result;
    }
    const result = call('editVehicle', editingPlate, model, rate);
    if (result.ok) {
      success('Updated vehicle ' + editingPlate + '.');
      refreshAll();
      fleetTable.select(editingPlate);
    }
    return result;
  });

  function editVehicle(plate) {
    const key = plate || fleetTable.selectedKey;
    if (!key) { setLastAction('Select a vehicle first.'); return; }
    if (!session.can.manageFleet) { reject('Access denied: only a Manager may add, edit or remove vehicles.'); return; }
    const check = call('checkEditable', key); // the C++ logic refuses rented vehicles
    if (!check.ok) { reject(check.message); return; }
    openVehicleDialog(check.vehicle);
  }

  async function removeVehicle() {
    const row = fleetTable.selectedRow();
    if (!row) { setLastAction('Select a vehicle first.'); return; }
    const vehicle = current.vehicles.find((item) => item.plate === row.key);
    const ok = await confirmAction('Remove vehicle',
      'Remove ' + row.key + (vehicle ? ' (' + vehicle.model + ')' : '') + ' from the fleet?\n\n' +
      'Its completed rentals stay in the rental history. Choose Save to make the change permanent.',
      'Remove vehicle');
    if (!ok) return;
    const result = call('removeVehicle', row.key); // the C++ logic checks the role and refuses rented vehicles
    if (!result.ok) { reject(result.message); return; }
    success('Removed vehicle ' + row.key + '.');
    refreshAll();
  }

  let rentingPlate = null;

  function rentVehicle(plate) {
    const key = plate || searchTable.selectedKey;
    if (!key) { setLastAction('Select an available vehicle first.'); return; }
    const check = call('checkRentable', key);
    if (!check.ok) { reject(check.message); return; }
    const vehicle = check.vehicle;
    const dialog = $('rentDialog');
    const form = dialog.querySelector('form');
    prepareDialog(dialog);
    rentingPlate = vehicle.plate;
    fillDetails($('rentDetails'), [['License plate', vehicle.plate], ['Model', vehicle.model], ['Current daily rate', vehicle.rate]]);
    form.elements.renter.value = '';
    form.elements.phone.value = '';
    form.elements.agreed.value = vehicle.ratePlain;
    dialog.showModal();
    form.elements.renter.focus();
  }

  setupDialog($('rentDialog'), (form) => {
    const result = call('rentVehicle', rentingPlate, form.elements.renter.value, form.elements.phone.value, form.elements.agreed.value);
    if (result.ok) {
      success('Rented ' + rentingPlate + ' to ' + result.renter + '. The start date was recorded.');
      refreshAll();
    }
    return result;
  });

  let returningPlate = null;
  let lastReceipt = null;

  function updatePreview() {
    const days = $('returnDialog').querySelector('form').elements.days.value;
    const preview = call('previewCharge', returningPlate, days); // Rental::calculateCharge() in C++
    $('chargePreview').textContent = preview.ok
      ? 'Charge: ' + preview.days + (preview.days === 1 ? ' day ' : ' days ') + TIMES + ' ' + preview.rate + ' = ' + preview.charge
      : 'Charge: enter a whole number of days from 1 to 3650.';
  }

  function returnVehicle(plate) {
    const key = plate || rentalsTable.selectedKey;
    if (!key) { setLastAction('Select a rented vehicle first.'); return; }
    const check = call('checkReturnable', key);
    if (!check.ok) { reject(check.message); return; }
    const vehicle = check.vehicle;
    const suggested = call('suggestedDays', key); // calculated by C++ from the rental start date
    const dialog = $('returnDialog');
    const form = dialog.querySelector('form');
    prepareDialog(dialog);
    returningPlate = vehicle.plate;
    fillDetails($('returnDetails'), [
      ['License plate', vehicle.plate], ['Model', vehicle.model], ['Renter', vehicle.renter],
      ['Telephone', vehicle.phone], ['Rented since', vehicle.startDate || 'unknown'], ['Agreed daily rate', vehicle.agreed],
    ]);
    form.elements.days.value = String(suggested.ok ? suggested.days : 1);
    updatePreview();
    dialog.showModal();
    form.elements.days.focus();
    form.elements.days.select();
  }

  setupDialog($('returnDialog'), (form) => {
    const plate = returningPlate;
    const result = call('returnVehicle', plate, form.elements.days.value);
    if (result.ok) {
      const receipt = result.receipt;
      success('Returned ' + plate + ' - charged ' + receipt.total + ' (receipt no. ' + receipt.number + ').');
      refreshAll();
      setTimeout(() => showReceipt(receipt, false), 0);
    }
    return result;
  });

  (function setupDaysInput() {
    const input = $('returnDialog').querySelector('form').elements.days;
    // Digits only while typing (like ES_NUMBER in the desktop program); pasting is
    // still possible so the C++ validation of "2.5", "-3" or "abc" can be shown.
    input.addEventListener('beforeinput', (event) => {
      if (event.inputType === 'insertText' && event.data && /\D/.test(event.data)) event.preventDefault();
    });
    input.addEventListener('input', updatePreview);
    input.addEventListener('keydown', (event) => {
      if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
        event.preventDefault();
        stepDays(event.key === 'ArrowUp' ? 1 : -1);
      }
    });
    $('returnDialog').querySelectorAll('.spin-btn').forEach((button) =>
      button.addEventListener('click', () => stepDays(Number(button.dataset.step))));
    function stepDays(step) {
      const value = /^\d+$/.test(input.value) ? Number(input.value) : 1;
      input.value = String(Math.min(3650, Math.max(1, value + step)));
      updatePreview();
    }
  })();

  function receiptRows(receipt) {
    const rows = [['License plate', receipt.plate], ['Model', receipt.model], ['Renter', receipt.renter], ['Telephone', receipt.phone]];
    if (receipt.startedOn) rows.push(['Rented since', receipt.startedOn]);
    rows.push(['Returned', receipt.returnedOn], ['Agreed daily rate', receipt.rate], ['Rental days', String(receipt.days)]);
    rows.push(['Total charge', receipt.rate + ' ' + TIMES + ' ' + receipt.days + ' = ' + receipt.total]);
    return rows;
  }

  function showReceipt(receipt, isCopy) {
    $('receiptTitle').textContent = isCopy ? 'Rental Receipt (copy)' : 'Rental Receipt';
    $('receiptMeta').textContent = 'Receipt no. ' + receipt.number + ' · returned ' + receipt.returnedOn;
    fillDetails($('receiptDetails'), receiptRows(receipt));
    lastReceipt = receipt;
    $('receiptDialog').showModal();
  }

  function showHistoryReceipt(key) {
    const number = key || historyTable.selectedKey;
    const record = historyRecords.find((item) => String(item.number) === number);
    if (!record) { setLastAction('Select a record first.'); return; }
    showReceipt(record, true);
  }

  function printReceipt() {
    if (!lastReceipt) return;
    const receipt = lastReceipt;
    const win = window.open('', '_blank', 'width=480,height=640');
    if (!win) { showMessage('Print', 'Please allow pop-ups for this site to print the receipt.'); return; }
    const doc = win.document;
    doc.title = 'Rental Receipt ' + receipt.number;
    const style = doc.createElement('style');
    style.textContent = 'body{font:14px/1.6 "Segoe UI",Arial,sans-serif;margin:32px;color:#111}' +
      'h1{font-size:20px;margin:0 0 4px}p{margin:0 0 20px;color:#555}table{border-collapse:collapse;width:100%}' +
      'td{padding:6px 0;border-bottom:1px solid #ddd}td+td{text-align:right;font-weight:600}tr:last-child td{font-size:16px;border-bottom:0;padding-top:12px}';
    doc.head.appendChild(style);
    const heading = doc.createElement('h1');
    heading.textContent = 'Car Rental Management System';
    const sub = doc.createElement('p');
    sub.textContent = 'Rental receipt no. ' + receipt.number + ' – returned ' + receipt.returnedOn;
    const table = doc.createElement('table');
    for (const [label, value] of receiptRows(receipt)) {
      const tr = table.insertRow();
      tr.insertCell().textContent = label;
      tr.insertCell().textContent = value;
    }
    doc.body.append(heading, sub, table);
    win.focus();
    win.print();
  }

  // ------------------------------------------------------------------ users --
  const addUserForm = $('addUserForm');
  addUserForm.addEventListener('submit', (event) => {
    event.preventDefault();
    const name = addUserForm.elements.username.value;
    const result = call('addUser', name, addUserForm.elements.password.value, addUserForm.elements.role.value);
    if (!result.ok) { showFormError(addUserForm, result); return; }
    clearFormError(addUserForm);
    addUserForm.reset();
    success('Account "' + name.trim().toLowerCase() + '" added. Save (Ctrl+S) to keep it.');
    refreshAll();
  });

  async function removeUser() {
    const row = usersTable.selectedRow();
    if (!row) return;
    const ok = await confirmAction('Delete account', 'Delete the account "' + row.key + '"?', 'Delete account');
    if (!ok) return;
    const result = call('removeUser', row.key); // C++: never yourself, never the last Manager
    if (!result.ok) { reject(result.message); return; }
    success('Account "' + row.key + '" deleted. Save (Ctrl+S) to keep the change.');
    refreshAll();
  }

  let resettingUser = null;
  function openResetPassword() {
    const row = usersTable.selectedRow();
    if (!row) return;
    resettingUser = row.key;
    const dialog = $('resetPasswordDialog');
    prepareDialog(dialog);
    $('resetPasswordFor').textContent = 'New password for "' + row.key + '":';
    dialog.querySelector('form').elements.password.value = '';
    dialog.showModal();
    dialog.querySelector('form').elements.password.focus();
  }
  setupDialog($('resetPasswordDialog'), (form) => {
    const result = call('resetPassword', resettingUser, form.elements.password.value);
    if (result.ok) success('Password of "' + resettingUser + '" changed. Save (Ctrl+S) to keep it.');
    refreshAll();
    return result;
  });

  function openChangePassword() {
    const dialog = $('passwordDialog');
    const form = dialog.querySelector('form');
    prepareDialog(dialog);
    form.reset();
    dialog.showModal();
    form.elements.current.focus();
  }
  setupDialog($('passwordDialog'), (form) => {
    if (form.elements.password.value !== form.elements.confirm.value) {
      return { ok: false, field: 'confirm', message: 'The new password and its confirmation are different.' };
    }
    const result = call('changePassword', form.elements.current.value, form.elements.password.value);
    if (!result.ok && result.field === 'password' && /current/i.test(result.message)) result.field = 'current';
    if (result.ok) {
      session = result;
      if (credentials) credentials.password = form.elements.password.value;
      success('Your password was changed. Save (Ctrl+S) to keep the change.');
      refreshAll();
    }
    return result;
  });

  // -------------------------------------------------------------- save/load --
  async function save() {
    const result = call('saveBundle'); // C++: RentalAgency::saveTo(IDataStore&) -> BundleDataStore
    if (!result.ok) { reject('The data could not be saved. ' + result.message); return false; }
    if (mode === 'online') {
      const upload = await uploadShared(result.bundle);
      if (!upload.ok) {
        app.markUnsaved();
        refreshAll();
        reject(upload.message);
        return false;
      }
      success('Saved to the shared online database (version ' + remoteVersion + ').');
    } else {
      const stored = storageSet(STORAGE_BUNDLE, result.bundle);
      OLD_KEYS.forEach(storageRemove);
      success(stored ? 'Saved ' + countText(current.total, 'vehicle', 'vehicles') + ', the revenue, the history and the user accounts.'
                     : 'Saved for this session only (browser storage is unavailable).');
    }
    refreshAll();
    return true;
  }

  // Loads a bundle into the C++ engine and shows any skipped lines.
  function loadBundle(bundle, sourceLabel) {
    const result = call('loadBundle', bundle); // RentalAgency::loadFrom(IDataStore&)
    if (!result.ok) throw new Error(result.message);
    [fleetTable, searchTable, rentalsTable, historyTable, usersTable].forEach((table) => { table.selectedKey = null; });
    setLastAction('Loaded the data from ' + sourceLabel + '.');
    return result.warnings;
  }

  function showLoadWarnings(warnings) {
    if (!warnings || warnings.length === 0) return;
    const lines = warnings.map((warning) => '• ' + warning).join('\n');
    showMessage('Some data was skipped',
      countText(warnings.length, 'entry', 'entries') + ' in the data could not be loaded and ' +
      (warnings.length === 1 ? 'was' : 'were') + ' skipped:\n\n' + lines +
      '\n\nAll other data was loaded normally. Saving will rewrite the data without the skipped lines.');
  }

  async function confirmDiscard() {
    return !current.modified ||
      confirmAction('Unsaved changes', 'You have unsaved changes. Discard them and continue?', 'Discard changes');
  }

  async function reload() {
    if (!(await confirmDiscard())) return;
    try {
      let warnings;
      if (mode === 'online') {
        const shared = await downloadShared(credentials);
        remoteVersion = shared.version;
        warnings = loadBundle(shared.bundle || await sampleBundle(), 'the shared online database');
      } else {
        warnings = loadBundle(await localBundle(), 'this browser');
      }
      session = call('session');
      if (!session.loggedIn) { showLogin(); return; } // the account no longer exists
      applyPermissions();
      refreshAll();
      success('Reloaded the latest data.');
      showLoadWarnings(warnings);
    } catch (error) {
      reject(error.message);
    }
  }

  // ------------------------------------------------------------------ login --
  const loginForm = $('loginForm');

  function showLogin() {
    session = { loggedIn: false, can: {} };
    $('loginScreen').hidden = false;
    clearFormError(loginForm);
    loginForm.elements.password.value = '';
    loginForm.elements.username.focus();
  }

  loginForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    clearFormError(loginForm);
    const username = loginForm.elements.username.value.trim().toLowerCase();
    const password = loginForm.elements.password.value;
    $('loginButton').disabled = true;
    try {
      let warnings = [];
      if (mode === 'online') {
        // The server checks the account before it sends any data.
        const shared = await downloadShared({ username, password });
        remoteVersion = shared.version;
        if (shared.bundle) {
          warnings = loadBundle(shared.bundle, 'the shared online database');
        } else {
          warnings = loadBundle(await sampleBundle(), 'the sample data (the shared database is empty)');
          app.markUnsaved(); // the first Save uploads it
        }
        credentials = { username, password };
      }
      const result = call('login', username, password); // C++ UserDirectory::authenticate
      if (!result.ok) { showFormError(loginForm, { message: result.message, field: 'password' }); return; }
      session = result;
      $('loginScreen').hidden = true;
      applyPermissions();
      refreshAll();
      setLastAction('Logged in as ' + session.username + ' (' + session.role + ').');
      showLoadWarnings(warnings);
      if (session.defaultPasswords && session.can.manageUsers) {
        showMessage('Security reminder',
          'The default accounts still use their default passwords (manager123 / clerk123).\n\n' +
          'Please change them on the Users page and save.');
      }
    } catch (error) {
      showFormError(loginForm, { message: error.message, field: 'password' });
    } finally {
      $('loginButton').disabled = false;
    }
  });

  async function logout() {
    $('userDropdown').hidden = true;
    if (current.modified) {
      const ok = await confirmAction('Log out', 'You have unsaved changes. Log out and discard them?', 'Discard and log out');
      if (!ok) return;
    }
    call('logout');
    credentials = null;
    if (mode === 'local') {
      try { loadBundle(await localBundle(), 'this browser'); } catch (error) { /* keep the current data */ }
    }
    current.modified = false;
    showLogin();
  }

  // ------------------------------------------------------------- navigation --
  const pages = ['dashboard', 'fleet', 'search', 'rentals', 'history', 'reports', 'users'];
  const focusTargets = { fleet: 'fleetSearch', search: 'searchInput', history: 'historySearch' };

  function currentPage() {
    return pages.find((page) => !$('page-' + page).hidden) || 'dashboard';
  }

  function selectPage(name, options = {}) {
    if (!pages.includes(name)) name = 'dashboard';
    const navItem = document.querySelector('.nav-item[data-page="' + name + '"]');
    if (navItem && navItem.hidden) name = 'dashboard'; // page not allowed for this role
    for (const page of pages) {
      $('page-' + page).hidden = page !== name;
    }
    document.querySelectorAll('.nav-item').forEach((item) => {
      if (item.dataset.page === name) item.setAttribute('aria-current', 'page');
      else item.removeAttribute('aria-current');
    });
    const section = $('page-' + name);
    $('pageTitle').textContent = section.dataset.title;
    $('pageSubtitle').textContent = section.dataset.subtitle;
    if (location.hash !== '#' + name) history.replaceState(null, '', '#' + name);
    closeSidebar();
    if (options.focus !== false && focusTargets[name]) $(focusTargets[name]).focus();
  }

  const scrim = document.createElement('div');
  scrim.className = 'scrim';
  document.body.appendChild(scrim);
  function closeSidebar() { $('sidebar').classList.remove('open'); scrim.classList.remove('open'); }
  function openSidebar() { $('sidebar').classList.add('open'); scrim.classList.add('open'); }

  // --------------------------------------------------------------- self-test --
  function runSelfTest() {
    const results = call('runSelfTest').results; // C++ BusinessRuleSelfTest
    const passed = results.filter((result) => result.passed).length;
    $('selfTestSummary').textContent = passed + ' of ' + results.length +
      ' business-rule checks passed (run on a temporary agency - your data was not changed).';
    const list = $('selfTestList');
    list.replaceChildren();
    for (const result of results) {
      const item = document.createElement('li');
      item.className = result.passed ? 'pass' : 'fail';
      const name = document.createElement('strong');
      name.textContent = (result.passed ? 'PASS  ' : 'FAIL  ') + result.name;
      const detail = document.createElement('span');
      detail.textContent = result.detail;
      item.append(name, detail);
      list.appendChild(item);
    }
    $('selfTestDialog').showModal();
    setLastAction('Self-test: ' + passed + ' of ' + results.length + ' passed.');
  }

  // ---------------------------------------------------------- wire-up events --
  function wireEvents() {
    if (wired) return;
    wired = true;
    document.querySelectorAll('.nav-item').forEach((item) =>
      item.addEventListener('click', () => selectPage(item.dataset.page)));
    document.querySelectorAll('[data-goto]').forEach((item) =>
      item.addEventListener('click', () => selectPage(item.dataset.goto)));
    $('menuToggle').addEventListener('click', openSidebar);
    scrim.addEventListener('click', closeSidebar);

    $('qaAdd').addEventListener('click', () => openVehicleDialog(null));
    $('addBtn').addEventListener('click', () => openVehicleDialog(null));
    $('editBtn').addEventListener('click', () => editVehicle());
    $('removeBtn').addEventListener('click', () => removeVehicle());
    $('rentBtn').addEventListener('click', () => rentVehicle());
    $('returnBtn').addEventListener('click', () => returnVehicle());
    $('historyReceiptBtn').addEventListener('click', () => showHistoryReceipt());
    $('printReceiptBtn').addEventListener('click', () => printReceipt());
    $('removeUserBtn').addEventListener('click', () => removeUser());
    $('resetPasswordBtn').addEventListener('click', () => openResetPassword());

    $('userChip').addEventListener('click', () => {
      const dropdown = $('userDropdown');
      dropdown.hidden = !dropdown.hidden;
      $('userChip').setAttribute('aria-expanded', dropdown.hidden ? 'false' : 'true');
    });
    document.addEventListener('click', (event) => {
      if (!event.target.closest('.user-menu')) $('userDropdown').hidden = true;
    });
    $('changePasswordBtn').addEventListener('click', () => { $('userDropdown').hidden = true; openChangePassword(); });
    $('logoutBtn').addEventListener('click', () => logout());

    $('fleetSearch').addEventListener('input', () => refreshAll());
    $('searchInput').addEventListener('input', () => refreshAll());
    $('historySearch').addEventListener('input', () => refreshAll());
    document.querySelectorAll('.segmented button').forEach((button) => button.addEventListener('click', () => {
      fleetFilter = button.dataset.filter;
      document.querySelectorAll('.segmented button').forEach((other) =>
        other.setAttribute('aria-checked', other === button ? 'true' : 'false'));
      refreshAll();
    }));
    $('fleetTable').addEventListener('keydown', (event) => {
      if (event.key === 'Delete' && fleetTable.selectedRow() && session.can.manageFleet) { event.preventDefault(); removeVehicle(); }
    });

    $('saveBtn').addEventListener('click', () => save());
    $('reloadBtn').addEventListener('click', () => reload());
    $('selfTestBtn').addEventListener('click', () => runSelfTest());
    $('errorsBtn').addEventListener('click', async () => {
      if (!(await confirmDiscard())) return;
      try {
        const bundle = await sampleBundle('data/vehicles_with_errors.txt');
        const keepUsers = call('saveBundle'); // keep the current accounts
        const users = keepUsers.ok ? keepUsers.bundle.split('--- users.txt')[1] || '' : '';
        const warnings = loadBundle(bundle.replace(/--- users\.txt\n[\s\S]*$/, '--- users.txt\n' + users.replace(/^\r?\n/, '')),
          'vehicles_with_errors.txt (not saved until you choose Save)');
        refreshAll(); // the C++ code keeps the current login when data is reloaded
        selectPage('fleet', { focus: false });
        showLoadWarnings(warnings);
      } catch (error) {
        reject(error.message);
      }
    });
    $('resetBtn').addEventListener('click', async () => {
      const ok = await confirmAction('Reset sample data',
        'Replace all vehicles, the revenue and the rental history with the original sample data?' +
        (mode === 'online' ? ' The change is uploaded to the shared database when you choose Save.' : ' Your saved data in this browser will be replaced when you choose Save.'),
        'Reset sample data');
      if (!ok) return;
      const keepUsers = call('saveBundle');
      const users = keepUsers.ok ? keepUsers.bundle.split('--- users.txt')[1] || '' : '';
      const sample = await sampleBundle();
      loadBundle(sample.replace(/--- users\.txt\n[\s\S]*$/, '--- users.txt\n' + users.replace(/^\r?\n/, '')), 'the sample data');
      app.markUnsaved();
      refreshAll();
      success('Sample data restored (user accounts kept). Choose Save to keep it.');
    });

    document.addEventListener('keydown', (event) => {
      if (!(event.ctrlKey || event.metaKey) || !session.loggedIn) return;
      const key = event.key.toLowerCase();
      if (key === 's') { event.preventDefault(); save(); }
      if (key === 'f') { event.preventDefault(); selectPage('search'); $('searchInput').select(); }
    });
    window.addEventListener('hashchange', () => selectPage(location.hash.slice(1), { focus: false }));
    window.addEventListener('beforeunload', (event) => {
      if (session.loggedIn && current.modified) { event.preventDefault(); event.returnValue = ''; }
    });
  }

  // ------------------------------------------------------------------- start --
  async function start() {
    selectPage(location.hash.slice(1) || 'dashboard', { focus: false });
    try {
      if (typeof window.createRentalModule !== 'function') throw new Error('rental_core.js was not loaded.');
      engine = await window.createRentalModule();
      app = new engine.WebApp();
      wireEvents();
      $('engineStatus').textContent = 'C++ engine: WebAssembly';
      $('engineStatus').classList.add('ready');

      mode = await detectMode();
      $('storageMode').textContent = mode === 'online' ? 'Shared online database' : 'Saved in this browser';
      $('storageMode').classList.toggle('online', mode === 'online');
      $('reloadBtn').hidden = mode !== 'online';
      if (mode === 'online') {
        $('loginMode').textContent = 'Shared online database - everybody works with the same data.';
        $('loginHint').textContent = 'First start: sign in with manager / manager123 (Manager) or clerk / clerk123 (Clerk), then change the passwords.';
      } else {
        const warnings = loadBundle(await localBundle(), 'this browser');
        $('loginMode').textContent = 'Data saved in this browser.';
        $('loginHint').textContent = 'Default accounts: manager / manager123 (Manager) and clerk / clerk123 (Clerk). Change the passwords after signing in.';
        if (warnings.length) setTimeout(() => showLoadWarnings(warnings), 0);
      }
      $('loginButton').disabled = false;
      showLogin();
    } catch (error) {
      $('engineStatus').textContent = 'The system could not be started: ' + error.message;
      $('engineStatus').classList.add('failed');
      $('loginMode').textContent = 'The system could not be started: ' + error.message;
    }
  }

  start();
})();
