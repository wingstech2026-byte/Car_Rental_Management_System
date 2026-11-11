/* =============================================================================
 * app.js - web user interface for the Car Rental Management System.
 * -----------------------------------------------------------------------------
 * This file contains NO business rules. Every operation is a call into the
 * C++ class WebApp (web/wasm/WebBindings.cpp), which uses the same C++
 * RentalAgency, Vehicle, Money, Validation, RentalStatistics and
 * TextFileDataStore classes as the desktop program, compiled to WebAssembly.
 * This file only draws the screen, like MainWindow and the dialogs do in the
 * Win32 version.
 * ========================================================================== */
'use strict';

(function () {
  const STORAGE_VEHICLES = 'crms.vehicles.txt';
  const STORAGE_REVENUE = 'crms.revenue.txt';
  const STORAGE_HISTORY = 'crms.history.txt';
  const DATA_DIR = '/data';
  const TIMES = '×';

  let engine = null;   // the compiled C++ module
  let app = null;      // C++ WebApp instance
  let lastAction = 'Ready.';

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

  // The C++ TextFileDataStore reads and writes these files in Emscripten's in-memory file system.
  function writeDataFiles(vehiclesText, revenueText, historyText) {
    engine.FS.writeFile(DATA_DIR + '/vehicles.txt', vehiclesText);
    engine.FS.writeFile(DATA_DIR + '/revenue.txt', revenueText);
    engine.FS.writeFile(DATA_DIR + '/history.txt', historyText);
  }

  function readDataFile(name) {
    try { return engine.FS.readFile(DATA_DIR + '/' + name, { encoding: 'utf8' }); } catch (error) { return ''; }
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

  function setupDialog(dialog, onOk) {
    const form = dialog.querySelector('form');
    const errorBox = dialog.querySelector('.dlg-error');
    form.addEventListener('submit', (event) => {
      event.preventDefault();
      const result = onOk(form);
      if (result.ok) {
        errorBox.hidden = true;
        dialog.close('ok');
        return;
      }
      // Same behaviour as ModalDialog::handleOk() in the desktop program: show the
      // message, keep the dialog open and focus the field the C++ exception refers to.
      errorBox.textContent = result.message;
      errorBox.hidden = false;
      form.querySelectorAll('.invalid').forEach((input) => input.classList.remove('invalid'));
      const field = result.field ? form.elements[result.field] : null;
      if (field && !field.readOnly) {
        field.classList.add('invalid');
        field.focus();
        field.select();
      }
    });
    dialog.querySelectorAll('[data-cancel]').forEach((button) =>
      button.addEventListener('click', () => dialog.close('cancel')));
  }

  function prepareDialog(dialog) {
    const errorBox = dialog.querySelector('.dlg-error');
    if (errorBox) { errorBox.hidden = true; errorBox.textContent = ''; }
    dialog.querySelectorAll('.invalid').forEach((input) => input.classList.remove('invalid'));
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

  let current = { vehicles: [], revenue: '0.00', total: 0, available: 0, rented: 0, modified: false };
  let historyRecords = [];
  let fleetFilter = 'all';

  // ---------------------------------------------------------------- refresh --
  function refreshAll() {
    current = call('state');
    const stats = call('statistics'); // C++ RentalStatistics
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
    rentalsTable.setRows(current.vehicles.filter((vehicle) => vehicle.status === 'Rented').map((vehicle) => ({
      key: vehicle.plate,
      cells: [
        { text: vehicle.plate, sort: vehicle.plate, strong: true },
        { text: vehicle.model, sort: vehicle.model },
        { text: vehicle.renter, sort: vehicle.renter },
        { text: vehicle.phone, sort: vehicle.phone },
        { text: vehicle.agreed, sort: vehicle.agreedCents },
      ],
    })));
    $('revenueLabel').textContent = current.revenue;
    $('dailyIncomeLabel').textContent = stats.activeDailyIncome;

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
    refreshReports(stats);

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
    $('kTotal').textContent = String(stats.vehicles);
    $('kTotalNote').textContent = 'in the fleet';
    $('kAvailable').textContent = String(stats.available);
    $('kRented').textContent = String(stats.rented);
    $('kUtilization').textContent = stats.utilization + '% of the fleet';
    $('kRevenue').textContent = stats.revenue;
    $('kRevenueNote').textContent = 'from ' + countText(stats.completedRentals, 'completed rental', 'completed rentals');
    $('utilFill').style.width = stats.utilization + '%';
    $('utilText').textContent = stats.utilization + '% rented · active rentals earn ' + stats.activeDailyIncome + ' per day';
    $('utilMeter').setAttribute('aria-label', stats.utilization + ' percent of the fleet is rented');

    const list = $('recentList');
    list.replaceChildren();
    const recent = historyRecordsAll().slice(-5).reverse();
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
      detail.textContent = record.plate + ' · ' + record.model + ' · ' + record.days + (record.days === 1 ? ' day' : ' days') + ' · ' + record.returnedOn;
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

  function updateButtons() {
    const fleetRow = fleetTable.selectedRow();
    $('editBtn').disabled = !(fleetRow && fleetRow.available);
    $('removeBtn').disabled = !(fleetRow && fleetRow.available);
    $('rentBtn').disabled = !searchTable.selectedRow();
    $('returnBtn').disabled = !rentalsTable.selectedRow();
    $('historyReceiptBtn').disabled = !historyTable.selectedRow();
  }

  // -------------------------------------------------------------- use cases --
  let editingPlate = null; // null = add mode

  function openVehicleDialog(vehicle) {
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
    const result = call('removeVehicle', row.key); // the C++ logic refuses rented vehicles
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
      success('Rented ' + rentingPlate + ' to ' + result.renter + '.');
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
    const dialog = $('returnDialog');
    const form = dialog.querySelector('form');
    prepareDialog(dialog);
    returningPlate = vehicle.plate;
    fillDetails($('returnDetails'), [
      ['License plate', vehicle.plate], ['Model', vehicle.model], ['Renter', vehicle.renter],
      ['Telephone', vehicle.phone], ['Agreed daily rate', vehicle.agreed],
    ]);
    form.elements.days.value = '1';
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

  function showReceipt(receipt, isCopy) {
    $('receiptTitle').textContent = isCopy ? 'Rental Receipt (copy)' : 'Rental Receipt';
    $('receiptMeta').textContent = 'Receipt no. ' + receipt.number + ' · returned ' + receipt.returnedOn;
    fillDetails($('receiptDetails'), [
      ['License plate', receipt.plate], ['Model', receipt.model], ['Renter', receipt.renter],
      ['Telephone', receipt.phone], ['Agreed daily rate', receipt.rate], ['Rental days', String(receipt.days)],
      ['Total charge', receipt.rate + ' ' + TIMES + ' ' + receipt.days + ' = ' + receipt.total],
    ]);
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
    const rows = [
      ['License plate', receipt.plate], ['Model', receipt.model], ['Renter', receipt.renter],
      ['Telephone', receipt.phone], ['Agreed daily rate', receipt.rate], ['Rental days', String(receipt.days)],
      ['TOTAL CHARGE', receipt.rate + ' ' + TIMES + ' ' + receipt.days + ' = ' + receipt.total],
    ];
    const win = window.open('', '_blank', 'width=480,height=600');
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
    for (const [label, value] of rows) {
      const tr = table.insertRow();
      tr.insertCell().textContent = label;
      tr.insertCell().textContent = value;
    }
    doc.body.append(heading, sub, table);
    win.focus();
    win.print();
  }

  // -------------------------------------------------------------- save/load --
  function save() {
    const result = call('save'); // TextFileDataStore::save() through IDataStore&
    if (!result.ok) { reject('The data could not be saved. ' + result.message); return false; }
    const stored = storageSet(STORAGE_VEHICLES, readDataFile('vehicles.txt')) &&
                   storageSet(STORAGE_REVENUE, readDataFile('revenue.txt')) &&
                   storageSet(STORAGE_HISTORY, readDataFile('history.txt'));
    success(stored
      ? 'Saved ' + countText(current.total, 'vehicle', 'vehicles') + ', revenue ' + current.revenue + ' and the rental history.'
      : 'Saved for this session only (browser storage is unavailable).');
    refreshAll();
    return true;
  }

  function loadFromFiles(vehiclesText, revenueText, historyText, sourceLabel) {
    writeDataFiles(vehiclesText, revenueText, historyText);
    const result = call('load'); // RentalAgency::loadFrom(IDataStore&)
    if (!result.ok) { reject(result.message); return; }
    [fleetTable, searchTable, rentalsTable, historyTable].forEach((table) => { table.selectedKey = null; });
    refreshAll();
    setLastAction('Loaded ' + countText(current.total, 'vehicle', 'vehicles') + ' from ' + sourceLabel + '.');
    if (result.warnings.length > 0) {
      const lines = result.warnings.map((warning) => '• ' + warning).join('\n');
      showMessage('Some data was skipped',
        countText(result.warnings.length, 'entry', 'entries') + ' in the data files could not be loaded and ' +
        (result.warnings.length === 1 ? 'was' : 'were') + ' skipped:\n\n' + lines +
        '\n\nAll other data was loaded normally. Saving will rewrite the files without the skipped lines.');
    }
  }

  async function loadSample() {
    const [vehicles, revenue, history] = await Promise.all([
      fetchText('data/vehicles.txt'), fetchText('data/revenue.txt'), fetchText('data/history.txt')]);
    loadFromFiles(vehicles, revenue, history, 'the sample data');
  }

  async function confirmDiscard() {
    return !current.modified ||
      confirmAction('Unsaved changes', 'You have unsaved changes. Discard them and continue?', 'Discard changes');
  }

  // ------------------------------------------------------------- navigation --
  const pages = ['dashboard', 'fleet', 'search', 'rentals', 'history', 'reports'];
  const focusTargets = { fleet: 'fleetSearch', search: 'searchInput', history: 'historySearch' };

  function selectPage(name, options = {}) {
    if (!pages.includes(name)) name = 'dashboard';
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
      if (event.key === 'Delete' && fleetTable.selectedRow()) { event.preventDefault(); removeVehicle(); }
    });

    $('saveBtn').addEventListener('click', () => save());
    $('selfTestBtn').addEventListener('click', () => runSelfTest());
    $('errorsBtn').addEventListener('click', async () => {
      if (!(await confirmDiscard())) return;
      const [vehicles, revenue, history] = await Promise.all([
        fetchText('data/vehicles_with_errors.txt'), fetchText('data/revenue.txt'), fetchText('data/history.txt')]);
      loadFromFiles(vehicles, revenue, history, 'vehicles_with_errors.txt (not saved until you choose Save)');
      selectPage('fleet', { focus: false });
    });
    $('resetBtn').addEventListener('click', async () => {
      const ok = await confirmAction('Reset sample data',
        'Replace all vehicles, the revenue and the rental history with the original sample data? ' +
        'Your saved data in this browser will be deleted.', 'Reset sample data');
      if (!ok) return;
      storageRemove(STORAGE_VEHICLES);
      storageRemove(STORAGE_REVENUE);
      storageRemove(STORAGE_HISTORY);
      await loadSample();
      success('Sample data restored.');
    });

    document.addEventListener('keydown', (event) => {
      if (!(event.ctrlKey || event.metaKey) || !app) return;
      const key = event.key.toLowerCase();
      if (key === 's') { event.preventDefault(); save(); }
      if (key === 'f') { event.preventDefault(); selectPage('search'); $('searchInput').select(); }
    });
    window.addEventListener('hashchange', () => selectPage(location.hash.slice(1), { focus: false }));
    window.addEventListener('beforeunload', (event) => {
      if (current.modified) { event.preventDefault(); event.returnValue = ''; }
    });
  }

  function setControlsEnabled(enabled) {
    document.querySelectorAll('#app button, #app input').forEach((control) => { control.disabled = !enabled; });
  }

  // ------------------------------------------------------------------- start --
  async function start() {
    selectPage(location.hash.slice(1) || 'dashboard', { focus: false });
    setControlsEnabled(false);
    try {
      if (typeof window.createRentalModule !== 'function') throw new Error('rental_core.js was not loaded.');
      engine = await window.createRentalModule();
      engine.FS.mkdirTree(DATA_DIR);
      app = new engine.WebApp();
      setControlsEnabled(true);
      wireEvents();
      $('engineStatus').textContent = 'C++ engine: WebAssembly';
      $('engineStatus').classList.add('ready');
      const savedVehicles = storageGet(STORAGE_VEHICLES);
      if (savedVehicles !== null) {
        loadFromFiles(savedVehicles, storageGet(STORAGE_REVENUE) || '', storageGet(STORAGE_HISTORY) || '', 'your saved data');
      } else {
        await loadSample();
      }
    } catch (error) {
      $('engineStatus').textContent = 'The system could not be started: ' + error.message;
      $('engineStatus').classList.add('failed');
    }
  }

  start();
})();
