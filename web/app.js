/* =============================================================================
 * app.js - web user interface for the Car Rental Management System.
 * -----------------------------------------------------------------------------
 * This file contains NO business rules. Every operation is a call into the
 * C++ class WebApp (web/wasm/WebBindings.cpp), which uses the same C++
 * RentalAgency, Vehicle, Money, Validation and TextFileDataStore classes as the
 * desktop program, compiled to WebAssembly. This file only draws the screen,
 * like MainWindow and the dialogs do in the Win32 version.
 * ========================================================================== */
'use strict';

(function () {
  const STORAGE_VEHICLES = 'crms.vehicles.txt';
  const STORAGE_REVENUE = 'crms.revenue.txt';
  const STORAGE_HISTORY = 'crms.history.txt';
  const DATA_DIR = '/data';

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

  // Write the two data files into the C++ program's (in-memory) file system.
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

  // ------------------------------------------------------------- formatting --
  function countText(count, singular, plural) {
    return count + ' ' + (count === 1 ? singular : plural);
  }

  function setLastAction(text) {
    lastAction = String(text).replace(/\s+/g, ' ');
    $('sbAction').textContent = lastAction;
  }

  // ------------------------------------------------------------------ table --
  class ReportTable {
    constructor(tableId, options) {
      this.table = $(tableId);
      this.body = this.table.tBodies[0];
      this.columns = Array.from(this.table.tHead.rows[0].cells);
      this.rows = [];
      this.selectedKey = null;
      this.sortColumn = -1;
      this.ascending = true;
      this.onSelect = options.onSelect || (() => {});
      this.onActivate = options.onActivate || (() => {});
      this.emptyText = options.emptyText || 'No vehicles.';

      this.table.tabIndex = 0;
      this.columns.forEach((th, index) => {
        th.tabIndex = 0;
        th.setAttribute('role', 'columnheader');
        th.addEventListener('click', () => this.sortBy(index));
        th.addEventListener('keydown', (event) => {
          if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); this.sortBy(index); }
        });
      });
      this.body.addEventListener('click', (event) => {
        const tr = event.target.closest('tr[data-key]');
        if (tr) this.select(tr.dataset.key);
      });
      this.body.addEventListener('dblclick', (event) => {
        const tr = event.target.closest('tr[data-key]');
        if (tr) { this.select(tr.dataset.key); this.onActivate(tr.dataset.key); }
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
      }
      for (const row of rows) {
        const tr = document.createElement('tr');
        tr.dataset.key = row.key;
        if (row.className) tr.className = row.className;
        if (row.key === this.selectedKey) tr.classList.add('selected');
        tr.setAttribute('aria-selected', row.key === this.selectedKey ? 'true' : 'false');
        row.cells.forEach((cell, index) => {
          const td = document.createElement('td');
          td.textContent = cell.text;
          if (this.columns[index].classList.contains('num')) td.className = 'num';
          tr.appendChild(td);
        });
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
      if (event.target !== this.table) return;
      const rows = this.sortedRows();
      if (rows.length === 0) return;
      const index = rows.findIndex((row) => row.key === this.selectedKey);
      if (event.key === 'ArrowDown') {
        event.preventDefault();
        this.select(rows[Math.min(rows.length - 1, index + 1)].key);
      } else if (event.key === 'ArrowUp') {
        event.preventDefault();
        this.select(rows[Math.max(0, index - 1)].key);
      } else if (event.key === 'Enter' && this.selectedKey) {
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

  // Asks a yes/no question in the application's own style. Resolves to true for OK.
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
      // Same behaviour as ModalDialog::handleOk(): show the message, keep the
      // dialog open and focus the field the C++ ValidationException refers to.
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

  // ------------------------------------------------------------------ state --
  const fleetTable = new ReportTable('fleetTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => editVehicle(key),
    emptyText: 'The fleet is empty. Click "Add Vehicle..." to add one.',
  });
  const searchTable = new ReportTable('searchTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => rentVehicle(key),
    emptyText: 'No available vehicle matches.',
  });
  const historyTable = new ReportTable('historyTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => showHistoryReceipt(key),
    emptyText: 'No completed rentals yet. Returned vehicles are archived here.',
  });
  let historyRecords = [];

  const rentalsTable = new ReportTable('rentalsTable', {
    onSelect: () => updateButtons(),
    onActivate: (key) => returnVehicle(key),
    emptyText: 'No vehicles are rented at the moment.',
  });

  let current = { vehicles: [], revenue: '0.00', total: 0, available: 0, rented: 0, modified: false };

  function refreshAll() {
    current = call('state');
    const byPlate = new Map(current.vehicles.map((vehicle) => [vehicle.plate, vehicle]));

    fleetTable.setRows(current.vehicles.map((vehicle) => ({
      key: vehicle.plate,
      className: vehicle.status === 'Available' ? 'row-ok' : 'row-rent',
      available: vehicle.status === 'Available',
      cells: [
        { text: vehicle.plate, sort: vehicle.plate },
        { text: vehicle.model, sort: vehicle.model },
        { text: vehicle.rate, sort: vehicle.rateCents },
        { text: vehicle.status, sort: vehicle.status },
      ],
    })));

    const query = $('searchInput').value.trim();
    const plates = call('search', query).plates;
    searchTable.setRows(plates.map((plate) => byPlate.get(plate)).map((vehicle) => ({
      key: vehicle.plate,
      cells: [
        { text: vehicle.plate, sort: vehicle.plate },
        { text: vehicle.model, sort: vehicle.model },
        { text: vehicle.rate, sort: vehicle.rateCents },
      ],
    })));
    $('searchCount').textContent = query === ''
      ? countText(plates.length, 'available vehicle', 'available vehicles') + ' (type to filter by model).'
      : countText(plates.length, 'available vehicle matches', 'available vehicles match') + ' "' + query + '".';

    rentalsTable.setRows(current.vehicles.filter((vehicle) => vehicle.status === 'Rented').map((vehicle) => ({
      key: vehicle.plate,
      cells: [
        { text: vehicle.plate, sort: vehicle.plate },
        { text: vehicle.model, sort: vehicle.model },
        { text: vehicle.renter, sort: vehicle.renter },
        { text: vehicle.phone, sort: vehicle.phone },
        { text: vehicle.agreed, sort: vehicle.agreedCents },
      ],
    })));
    $('revenueLabel').textContent = 'Cumulative Rental Revenue: ' + current.revenue;

    // Rental History tab (RentalAgency::searchHistory in C++)
    const historyQuery = $('historySearch').value.trim();
    const history = call('history', historyQuery);
    historyRecords = history.records;
    historyTable.setRows(historyRecords.map((record) => ({
      key: String(record.number),
      cells: [
        { text: String(record.number), sort: record.number },
        { text: record.returnedOn, sort: record.returnedOn },
        { text: record.plate, sort: record.plate },
        { text: record.model, sort: record.model },
        { text: record.renter, sort: record.renter },
        { text: record.phone, sort: record.phone },
        { text: record.rate, sort: record.rateCents },
        { text: String(record.days), sort: record.days },
        { text: record.total, sort: record.totalCents },
      ],
    })));
    $('historySummary').textContent = (historyQuery === ''
      ? countText(history.count, 'completed rental', 'completed rentals')
      : countText(history.count, 'record matches', 'records match') + ' "' + historyQuery + '"') +
      '   |   Total charged: ' + history.total;

    $('sbTotal').textContent = 'Total vehicles: ' + current.total;
    $('sbAvail').textContent = 'Available: ' + current.available;
    $('sbRented').textContent = 'Rented: ' + current.rented;
    $('sbRevenue').textContent = 'Revenue: ' + current.revenue;
    $('sbAction').textContent = lastAction;
    $('appTitle').textContent = 'Car Rental Management System' + (current.modified ? ' *' : '');
    document.title = (current.modified ? '* ' : '') + 'Car Rental Management System';
    updateButtons();
  }

  function updateButtons() {
    const fleetRow = fleetTable.selectedRow();
    $('editBtn').disabled = !(fleetRow && fleetRow.available);
    $('rentBtn').disabled = !searchTable.selectedRow();
    $('returnBtn').disabled = !rentalsTable.selectedRow();
    $('historyReceiptBtn').disabled = !historyTable.selectedRow();
  }

  function reject(message) {
    setLastAction('Rejected: ' + message);
    showMessage('Car Rental Management System', message);
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
    const plate = form.elements.plate.value;
    const model = form.elements.model.value;
    const rate = form.elements.rate.value;
    if (editingPlate === null) {
      const result = call('addVehicle', plate, model, rate);
      if (result.ok) {
        setLastAction('Added vehicle ' + result.plate + '.');
        refreshAll();
        selectTab('fleet');
        fleetTable.select(result.plate);
      }
      return result;
    }
    const result = call('editVehicle', editingPlate, model, rate);
    if (result.ok) {
      setLastAction('Updated vehicle ' + editingPlate + '.');
      refreshAll();
      fleetTable.select(editingPlate);
    }
    return result;
  });

  function editVehicle(plate) {
    const key = plate || fleetTable.selectedKey;
    if (!key) { setLastAction('Select a vehicle on the Fleet tab first.'); return; }
    const check = call('checkEditable', key); // C++ refuses rented vehicles
    if (!check.ok) { reject(check.message); return; }
    openVehicleDialog(check.vehicle);
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
    form.elements.agreed.value = vehicle.ratePlain; // pre-filled, editable
    dialog.showModal();
    form.elements.renter.focus();
  }

  setupDialog($('rentDialog'), (form) => {
    const result = call('rentVehicle', rentingPlate, form.elements.renter.value, form.elements.phone.value, form.elements.agreed.value);
    if (result.ok) {
      setLastAction('Rented ' + rentingPlate + ' to ' + result.renter + '.');
      refreshAll();
    }
    return result;
  });

  let returningPlate = null;
  let lastReceipt = null;

  // Prints the last receipt (opens a small printable page).
  function printReceipt() {
    if (!lastReceipt) return;
    const receipt = lastReceipt;
    const rows = [
      ['License plate', receipt.plate], ['Model', receipt.model], ['Renter', receipt.renter],
      ['Telephone', receipt.phone], ['Agreed daily rate', receipt.rate], ['Rental days', String(receipt.days)],
      ['TOTAL CHARGE', receipt.rate + ' × ' + receipt.days + ' = ' + receipt.total],
    ];
    const win = window.open('', '_blank', 'width=480,height=600');
    if (!win) { showMessage('Print', 'Please allow pop-ups for this site to print the receipt.'); return; }
    const doc = win.document;
    doc.title = 'Rental Receipt ' + receipt.plate;
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

  function updatePreview() {
    const days = $('returnDialog').querySelector('form').elements.days.value;
    const preview = call('previewCharge', returningPlate, days); // Rental::calculateCharge() in C++
    $('chargePreview').textContent = preview.ok
      ? 'Charge: ' + preview.days + (preview.days === 1 ? ' day' : ' days') + ' × ' + preview.rate + ' = ' + preview.charge
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
      setLastAction('Returned ' + plate + ' - charged ' + receipt.total + ' (receipt no. ' + receipt.number + ').');
      refreshAll();
      setTimeout(() => showReceipt(receipt, false), 0);
    }
    return result;
  });

  // Shows a receipt: a new one after a return, or a copy from the Rental History.
  function showReceipt(receipt, isCopy) {
    $('receiptTitle').textContent = isCopy ? 'Rental Receipt (copy)' : 'Rental Receipt';
    $('receiptMeta').textContent = 'Receipt no. ' + receipt.number + ' – returned ' + receipt.returnedOn;
    fillDetails($('receiptDetails'), [
      ['License plate', receipt.plate], ['Model', receipt.model], ['Renter', receipt.renter],
      ['Telephone', receipt.phone], ['Agreed daily rate', receipt.rate], ['Rental days', String(receipt.days)],
      ['Total charge', receipt.rate + ' × ' + receipt.days + ' = ' + receipt.total],
    ]);
    lastReceipt = receipt;
    $('receiptDialog').showModal();
  }

  function showHistoryReceipt(key) {
    const number = key || historyTable.selectedKey;
    const record = historyRecords.find((item) => String(item.number) === number);
    if (!record) { setLastAction('Select a record on the Rental History tab first.'); return; }
    showReceipt(record, true);
  }

  // Days box: digits only while typing (like ES_NUMBER); pasting is still
  // possible so that the C++ validation of "2.5", "-3", "abc" can be shown.
  (function setupDaysInput() {
    const input = $('returnDialog').querySelector('form').elements.days;
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

  // -------------------------------------------------------------- save/load --
  function save() {
    const result = call('save'); // TextFileDataStore::save() through IDataStore&
    if (!result.ok) { reject('The data could not be saved. ' + result.message); return false; }
    const stored = storageSet(STORAGE_VEHICLES, readDataFile('vehicles.txt')) &&
                   storageSet(STORAGE_REVENUE, readDataFile('revenue.txt')) &&
                   storageSet(STORAGE_HISTORY, readDataFile('history.txt'));
    setLastAction(stored
      ? 'Saved ' + countText(current.total, 'vehicle', 'vehicles') + ' and revenue ' + current.revenue + '.'
      : 'Saved for this session only (browser storage is unavailable).');
    refreshAll();
    return true;
  }

  function loadFromFiles(vehiclesText, revenueText, historyText, sourceLabel) {
    writeDataFiles(vehiclesText, revenueText, historyText);
    const result = call('load'); // RentalAgency::loadFrom(IDataStore&)
    if (!result.ok) { reject(result.message); return; }
    fleetTable.selectedKey = null;
    searchTable.selectedKey = null;
    rentalsTable.selectedKey = null;
    historyTable.selectedKey = null;
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

  function readFileText(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result));
      reader.onerror = () => reject(new Error('Could not read ' + file.name));
      reader.readAsText(file, 'utf-8');
    });
  }

  // File > Open data files...: the user picks vehicles.txt and optionally revenue.txt
  // and history.txt (recognised by their names). Files not picked keep their current content.
  async function openDataFiles(fileList) {
    const files = Array.from(fileList);
    if (files.length === 0) return;
    if (!(await confirmDiscard())) return;
    const revenueFile = files.find((file) => /revenue/i.test(file.name));
    const historyFile = files.find((file) => /history/i.test(file.name));
    const vehicleFile = files.find((file) => file !== revenueFile && file !== historyFile);
    const vehiclesText = vehicleFile ? await readFileText(vehicleFile) : readDataFile('vehicles.txt');
    const revenueText = revenueFile ? await readFileText(revenueFile) : readDataFile('revenue.txt');
    const historyText = historyFile ? await readFileText(historyFile) : readDataFile('history.txt');
    loadFromFiles(vehiclesText, revenueText, historyText, files.map((file) => file.name).join(' + '));
    // Keep the opened files as the saved data of this browser.
    storageSet(STORAGE_VEHICLES, vehiclesText);
    storageSet(STORAGE_REVENUE, revenueText);
    storageSet(STORAGE_HISTORY, historyText);
  }

  function downloadText(fileName, text) {
    const url = URL.createObjectURL(new Blob([text], { type: 'text/plain;charset=utf-8' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = fileName;
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  // File > Download data files: saves, then downloads vehicles.txt and revenue.txt
  // exactly as written by the C++ TextFileDataStore.
  function downloadDataFiles() {
    if (!save()) return;
    downloadText('vehicles.txt', readDataFile('vehicles.txt'));
    setTimeout(() => downloadText('revenue.txt', readDataFile('revenue.txt')), 300);
    setTimeout(() => downloadText('history.txt', readDataFile('history.txt')), 600);
    setLastAction('Saved and downloaded vehicles.txt, revenue.txt and history.txt.');
  }

  // ------------------------------------------------------------------ menus --
  function setupMenus() {
    const menus = Array.from(document.querySelectorAll('.menu'));
    const closeAll = (except) => menus.forEach((menu) => {
      if (menu === except) return;
      menu.querySelector('.menu-list').hidden = true;
      menu.querySelector('.menu-button').setAttribute('aria-expanded', 'false');
    });
    for (const menu of menus) {
      const button = menu.querySelector('.menu-button');
      const list = menu.querySelector('.menu-list');
      button.addEventListener('click', () => {
        const open = list.hidden;
        closeAll(menu);
        list.hidden = !open;
        button.setAttribute('aria-expanded', open ? 'true' : 'false');
        if (open) list.querySelector('[role=menuitem]').focus();
      });
      list.addEventListener('click', (event) => { if (event.target.closest('[role=menuitem]')) closeAll(); });
      list.addEventListener('keydown', (event) => {
        const items = Array.from(list.querySelectorAll('[role=menuitem]'));
        const index = items.indexOf(document.activeElement);
        if (event.key === 'ArrowDown') { event.preventDefault(); items[(index + 1) % items.length].focus(); }
        if (event.key === 'ArrowUp') { event.preventDefault(); items[(index - 1 + items.length) % items.length].focus(); }
        if (event.key === 'Escape') { event.preventDefault(); closeAll(); button.focus(); }
      });
    }
    document.addEventListener('click', (event) => { if (!event.target.closest('.menu')) closeAll(); });
  }

  // ------------------------------------------------------------------- tabs --
  const tabs = ['fleet', 'search', 'rentals', 'history'];

  function selectTab(name) {
    for (const tab of tabs) {
      const active = tab === name;
      const button = $('tab-' + tab);
      button.setAttribute('aria-selected', active ? 'true' : 'false');
      button.tabIndex = active ? 0 : -1;
      $('panel-' + tab).hidden = !active;
    }
    if (name === 'search') $('searchInput').focus();
    if (name === 'history') $('historySearch').focus();
  }

  tabs.forEach((tab, index) => {
    const button = $('tab-' + tab);
    button.addEventListener('click', () => selectTab(tab));
    button.addEventListener('keydown', (event) => {
      if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') {
        event.preventDefault();
        const next = tabs[(index + (event.key === 'ArrowRight' ? 1 : tabs.length - 1)) % tabs.length];
        selectTab(next);
        $('tab-' + next).focus();
      }
    });
  });

  // ---------------------------------------------------------- wire-up events --
  function wireEvents() {
    $('addBtn').addEventListener('click', () => openVehicleDialog(null));
    $('editBtn').addEventListener('click', () => editVehicle());
    $('rentBtn').addEventListener('click', () => rentVehicle());
    $('returnBtn').addEventListener('click', () => returnVehicle());
    $('searchInput').addEventListener('input', () => refreshAll());
    $('historySearch').addEventListener('input', () => refreshAll());
    $('historyReceiptBtn').addEventListener('click', () => showHistoryReceipt());
    $('saveBtn').addEventListener('click', () => save());
    $('resetBtn').addEventListener('click', async () => {
      const ok = await confirmAction('Restore sample data',
        'Replace all vehicles, the revenue and the rental history with the original sample data? ' +
        'Your saved data in this browser will be deleted.',
        'Restore sample data');
      if (!ok) return;
      storageRemove(STORAGE_VEHICLES);
      storageRemove(STORAGE_REVENUE);
      storageRemove(STORAGE_HISTORY);
      await loadSample();
    });
    $('openBtn').addEventListener('click', () => $('fileInput').click());
    $('fileInput').addEventListener('change', async (event) => {
      try { await openDataFiles(event.target.files); } catch (error) { reject(error.message); }
      event.target.value = '';
    });
    $('downloadBtn').addEventListener('click', () => downloadDataFiles());
    $('printReceiptBtn').addEventListener('click', () => printReceipt());
    setupMenus();
    document.addEventListener('keydown', (event) => {
      if (!(event.ctrlKey || event.metaKey) || !app) return;
      const key = event.key.toLowerCase();
      if (key === 's') { event.preventDefault(); save(); }
      if (key === 'f') { event.preventDefault(); selectTab('search'); $('searchInput').select(); }
    });
    window.addEventListener('beforeunload', (event) => {
      if (current.modified) { event.preventDefault(); event.returnValue = ''; }
    });
  }

  function setButtonsEnabled(enabled) {
    document.querySelectorAll('#app button').forEach((button) => { button.disabled = !enabled; });
  }

  // ------------------------------------------------------------------- start --
  async function start() {
    setButtonsEnabled(false);
    try {
      if (typeof window.createRentalModule !== 'function') throw new Error('rental_core.js was not loaded.');
      engine = await window.createRentalModule();
      engine.FS.mkdirTree(DATA_DIR);
      app = new engine.WebApp();
      setButtonsEnabled(true);
      wireEvents();
      const savedVehicles = storageGet(STORAGE_VEHICLES);
      const savedRevenue = storageGet(STORAGE_REVENUE);
      const savedHistory = storageGet(STORAGE_HISTORY);
      if (savedVehicles !== null) {
        loadFromFiles(savedVehicles, savedRevenue || '', savedHistory || '', 'your saved data');
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
