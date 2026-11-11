# Test & Demonstration Checklist

This checklist covers every required function and every rejection rule. It gives the exact clicks, the inputs and the expected results.
Run the tests **in order**: the later revenue figures depend on the earlier steps.

**Prepare a clean start**
1. Build the project (Debug | x64).
2. In `x64\Debug\`, delete `vehicles.txt` and `revenue.txt` if they exist.
3. Build again. The post-build step copies fresh sample data.
4. Press **F5**.

**Starting data** (from `data\vehicles.txt` and `data\revenue.txt`)

| Plate | Model | Daily Rate | Status | Renter / Phone / Agreed rate |
|---|---|---|---|---|
| ABC-123 | Toyota Corolla | 150.00 | Available | |
| BMW-520 | BMW 520i | 350.00 | Rented | David Lee / +44 20 7946 0958 / 320.00 |
| CAM-330 | Toyota Camry | 180.00 | Available | |
| COR-202 | Toyota Corolla | 155.00 | Rented | John Smith / +1 555-0100 / 140.00 |
| FOR-333 | Ford Focus | 120.00 | Available | |
| HON-111 | Honda Civic | 140.00 | Available | |
| HON-222 | Honda Civic | 145.50 | Rented | Maria Garcia / 0912-345-678 / 145.50 |
| TES-003 | Tesla Model 3 | 275.00 | Available | |

Cumulative revenue: **3,250.00**. Rental history: **4 completed rentals** (receipts 1-4), which together add up to 3,250.00.

Use the ☐ boxes to tick off each test.

---

## 0. Start-up and general appearance

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 0.1 | Start the program. | Window title is **"Car Rental Management System"** (no `*`). Modern themed controls in Segoe UI. No warning dialog appears. | ☐ |
| 0.2 | Look at the **Fleet** tab. | 8 vehicles with columns Plate / Model / Daily Rate / Status. Available rows are **light green**, Rented rows are **light orange**. Daily Rate is right-aligned with 2 decimals. | ☐ |
| 0.3 | Look at the status bar. | `Total vehicles: 8` \| `Available: 5` \| `Rented: 3` \| `Revenue: 3,250.00` \| `Loaded 8 vehicles from …` | ☐ |
| 0.4 | Drag the window corner to make it as small as possible. | It stops at about 1000×650 (scaled for the DPI). All controls move and resize with the window. | ☐ |
| 0.5 | Click the **Daily Rate** column header, then click it again. | The rows sort by rate ascending, then descending (numerically). An arrow is shown in the header. | ☐ |

## 1. Add vehicles

Fleet tab → click **Add Vehicle…** (or Alt+A). The dialog title is "Add Vehicle".

| # | Plate | Model | Daily rate | Expected result | ☐ |
|---|---|---|---|---|---|
| 1.1 | `kia-777` | `Kia Picanto` | `99.5` | Dialog closes. Fleet shows **KIA-777** (upper case), Kia Picanto, **99.50**, Available (green). Status bar: Total 9, Available 6, "Added vehicle KIA-777." Title now ends with **`*`**. | ☐ |
| 1.2 | `abc-123` | `Test` | `100` | Error: *"A vehicle with license plate ABC-123 already exists…"*. The dialog stays open with the **plate** field focused. (Duplicate check ignores case.) | ☐ |
| 1.3 | *(empty)* | `Test` | `100` | *"License plate must not be empty."* Focus on plate. | ☐ |
| 1.4 | `AB 123` | `Test` | `100` | *"License plate "AB 123" is invalid. Use only letters A-Z, digits 0-9 and '-' (no spaces)."* | ☐ |
| 1.5 | `AB_123` | `Test` | `100` | Same "invalid plate" error. | ☐ |
| 1.6 | `AB\|12` | `Test` | `100` | *"License plate must not contain the '\|' character…"* | ☐ |
| 1.7 | `---` | `Test` | `100` | *"License plate must contain at least one letter or digit."* | ☐ |
| 1.8 | `NEW-1` | *(empty)* | `100` | *"Vehicle model must not be empty."* Focus on **model**. | ☐ |
| 1.9 | `NEW-1` | `Kia\|Rio` | `100` | *"Vehicle model must not contain the '\|' character…"* | ☐ |
| 1.10 | `NEW-1` | `Kia Rio` | *(empty)* | *"Daily rate must not be empty."* Focus on **daily rate**. | ☐ |
| 1.11 | `NEW-1` | `Kia Rio` | `0` | *"Daily rate must be greater than 0.00."* | ☐ |
| 1.12 | `NEW-1` | `Kia Rio` | `-10` | *"Daily rate must be a positive number with at most 2 decimal places…"* | ☐ |
| 1.13 | `NEW-1` | `Kia Rio` | `12.345` | Same message (more than 2 decimals). | ☐ |
| 1.14 | `NEW-1` | `Kia Rio` | `abc` | Same message (letters). | ☐ |
| 1.15 | `NEW-1` | `Kia Rio` | `1,250.00` | Same message (thousands separators are not accepted on input). | ☐ |
| 1.16 | `NEW-1` | `Kia Rio` | `2000000` | *"Daily rate must not exceed 1,000,000.00."* | ☐ |
| 1.17 | Any input → press **Esc** | — | — | The dialog closes and nothing is added. | ☐ |

## 2. Edit vehicles (Available only, plate cannot change)

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 2.1 | Fleet tab: click **COR-202** (orange, Rented). | The **Edit Selected…** button is **disabled**. | ☐ |
| 2.2 | **Double-click** COR-202. | Error: *"Vehicle COR-202 is currently rented and cannot be edited. Return the vehicle first."* The status bar shows "Rejected: …". | ☐ |
| 2.3 | Click **KIA-777** (green). | **Edit Selected…** becomes enabled. | ☐ |
| 2.4 | Click **Edit Selected…** | The dialog title is "Edit Vehicle". The plate `KIA-777` is **read-only** (greyed) and the hint says "The license plate cannot be changed." Model and rate (`99.50`) are pre-filled, and the cursor is in Model. | ☐ |
| 2.5 | Set the rate to `0` → OK. | *"Daily rate must be greater than 0.00."* The dialog stays open and the rate field is focused. | ☐ |
| 2.6 | Set the model to *(empty)* → OK. | *"Vehicle model must not be empty."* Focus on model. | ☐ |
| 2.7 | Model `Kia Picanto GT`, rate `105` → press **Enter**. | The dialog closes. The row shows Kia Picanto GT / **105.00**. Status: "Updated vehicle KIA-777." | ☐ |
| 2.8 | Select KIA-777 and press **Enter** in the list. | The edit dialog opens (Enter = the main action). Press Esc to close it. | ☐ |

## 3. Search available vehicles by model

Click the **Search & Rent** tab (or press Ctrl+F).

| # | Type in the search box | Expected list | ☐ |
|---|---|---|---|
| 3.1 | *(empty; the grey cue text "Search model..." is visible)* | All **6** available vehicles: ABC-123, CAM-330, FOR-333, HON-111, KIA-777, TES-003. Label: "6 available vehicles (type to filter by model)." | ☐ |
| 3.2 | `cor` | **ABC-123 Toyota Corolla** only. COR-202 is also a Corolla, but it is rented and therefore not shown. The list updates **while typing**. | ☐ |
| 3.3 | `CIVIC` | **HON-111** only (case-insensitive; HON-222 is rented). | ☐ |
| 3.4 | `toyota` | ABC-123 and CAM-330 (partial match). | ☐ |
| 3.5 | `xyz` | Empty list. "0 available vehicles match "xyz"." **Rent Selected…** is disabled. | ☐ |
| 3.6 | Clear the box. | All 6 available vehicles appear again. | ☐ |

## 4. Rent an available vehicle

On the Search & Rent tab, search `cor`, select **ABC-123** and click **Rent Selected…** (or double-click, or press Enter).

| # | Input | Expected result | ☐ |
|---|---|---|---|
| 4.1 | *(open the dialog)* | Plate `ABC-123`, model and current rate `150.00` are shown **read-only**. "Agreed daily rate" is **pre-filled with `150.00`** and can be edited. The cursor is in Renter name. | ☐ |
| 4.2 | Name *(empty)*, phone `0912345678` → OK | *"Renter name must not be empty."* Focus on name. | ☐ |
| 4.3 | Name `Ann\|Lee` | *"Renter name must not contain the '\|' character…"* | ☐ |
| 4.4 | Name `Alice Brown`, phone *(empty)* | *"Telephone number must not be empty."* Focus on telephone. | ☐ |
| 4.5 | Phone `12345` | *"Telephone number must be 6 to 20 characters long (you entered 5)."* | ☐ |
| 4.6 | Phone `0912ab5678` | *"Telephone number may contain only digits, spaces, '-' and an optional leading '+'."* | ☐ |
| 4.7 | Phone `12+3456789` | Same message (`+` is only allowed at the start). | ☐ |
| 4.8 | Phone `------` | *"Telephone number must contain digits."* | ☐ |
| 4.9 | Try typing a 21-character phone number. | The box stops accepting characters after 20 (the 20-character limit). | ☐ |
| 4.10 | Phone `+1 555 0199`, agreed rate `0` | *"Agreed daily rate must be greater than 0.00."* Focus on agreed rate. | ☐ |
| 4.11 | Agreed rate `135.555` | *"Agreed daily rate must be a positive number with at most 2 decimal places…"* | ☐ |
| 4.12 | Agreed rate `135` → OK | The dialog closes. Status: "Rented ABC-123 to Alice Brown." The search list for `cor` is now **empty**. On the Fleet tab, ABC-123 is **orange / Rented**. Status bar: Available 5, Rented 4. | ☐ |
| 4.13 | **Rentals & Returns** tab | ABC-123 / Toyota Corolla / Alice Brown / +1 555 0199 / **135.00** (the agreed rate, which differs from the current rate of 150.00). | ☐ |

### Rule: a rented vehicle cannot be rented again

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 4.14 | Search tab, empty search. | ABC-123 is **not** listed, so the GUI offers no way to rent it again. | ☐ |
| 4.15 | **Help > Run Business Rule Self-Test** | Includes `[PASS] Reject renting a rented vehicle again`. The **business logic itself** throws `VehicleNotAvailableException` ("…is currently rented and cannot be rented again"), independent of the GUI. | ☐ |

## 5. Return a rented vehicle

Rentals & Returns tab → select **HON-222** (Maria Garcia, 145.50) → **Return Selected…**

| # | Days input | Expected result | ☐ |
|---|---|---|---|
| 5.1 | *(the dialog opens)* | It shows plate, model, renter, telephone and agreed rate 145.50 (all read-only). Days = `1` with an up/down spin control. Bold preview: **"Charge: 1 day × 145.50 = 145.50"**. | ☐ |
| 5.2 | Click the spin **up** arrow twice. | Days = 3. The preview updates live: **"Charge: 3 days × 145.50 = 436.50"**. | ☐ |
| 5.3 | Try typing a letter, `.` or `-`. | Windows blocks it ("Unacceptable character" balloon). The box accepts digits only. | ☐ |
| 5.4 | Delete the text (empty) → OK | *"Please enter the number of rental days."* The dialog stays open with the focus on days. The preview reads "Charge: enter a whole number of days from 1 to 3650." | ☐ |
| 5.5 | `0` → OK | *"Rental days must be at least 1."* | ☐ |
| 5.6 | Copy `2.5` (for example from Notepad) and paste it with Ctrl+V → OK | *"Rental days must be a positive whole number (digits only - no signs, decimals or letters)…"* | ☐ |
| 5.7 | Paste `-3` → OK | Same message. | ☐ |
| 5.8 | Paste `abc` → OK | Same message. *Note: on some Windows versions the digits-only box already refuses the paste. That is a rejection too, and the business-logic check is demonstrated by the self-test (section 11: "Reject rental days "2.5"" etc.).* | ☐ |
| 5.9 | `5000` → OK | *"Rental days must not exceed 3650 (10 years)."* | ☐ |
| 5.10 | `3` → OK | A **receipt** message box appears: plate HON-222, Honda Civic, Maria Garcia, 0912-345-678, agreed rate 145.50, 3 days, **TOTAL CHARGE: 145.50 x 3 = 436.50**. | ☐ |
| 5.11 | After closing the receipt | HON-222 has left the rentals list. The label shows **"Cumulative Rental Revenue: 3,686.50"** (3,250.00 + 436.50). On the Fleet tab HON-222 is green / Available. Status bar: Revenue 3,686.50. | ☐ |
| 5.12 | Return **ABC-123** for `4` days. | Preview "4 days × 135.00 = 540.00". The receipt total is 540.00, charged at the **agreed** rate and not the current 150.00. Revenue becomes **4,226.50**. | ☐ |
| 5.13 | Search tab, search `cor` | ABC-123 is available again. Renting it again shows an empty renter (the rental details were cleared). Cancel. | ☐ |

### Rule: only rented vehicles can be returned

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 5.14 | Rentals tab with nothing selected. | **Return Selected…** is disabled. Only rented vehicles are ever listed. | ☐ |
| 5.15 | Help > Run Business Rule Self-Test | `[PASS] Reject returning an available vehicle`. The business logic throws `VehicleNotRentedException`. | ☐ |

## 6. List rented vehicles and cumulative revenue

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 6.1 | Rentals & Returns tab | Exactly **BMW-520** and **COR-202**, with columns Plate, Model, Renter, Telephone, Agreed Rate (320.00 and 140.00, right-aligned). | ☐ |
| 6.2 | Revenue label | Large bold green text: **"Cumulative Rental Revenue: 4,226.50"**. | ☐ |
| 6.3 | Click the **Renter** header. | Sorts by renter name (David Lee, John Smith). | ☐ |

## 7. Money is always shown with two decimals

| # | Check | Expected | ☐ |
|---|---|---|---|
| 7.1 | KIA-777 rate typed as `105` | Displayed as **105.00**. | ☐ |
| 7.2 | Rate typed as `99.5` (test 1.1) | Was displayed as **99.50**. | ☐ |
| 7.3 | Revenue, receipt, status bar, preview | Always 2 decimals, with thousands separators (4,226.50). | ☐ |
| 7.4 | Self-test | `[PASS] Money is exact: 0.10 + 0.20 == 0.30` (stored as integer cents, no floating-point errors). | ☐ |

## 8. Saving, unsaved changes and exit

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 8.1 | Look at the title bar. | "Car Rental Management System **\***" (there are unsaved changes). | ☐ |
| 8.2 | Press **Ctrl+S** (or File > Save). | The `*` disappears. Status: "Saved 9 vehicles and revenue 4,226.50." | ☐ |
| 8.3 | Open `x64\Debug\vehicles.txt` in Notepad. | One line per vehicle, for example `KIA-777\|Kia Picanto GT\|105.00\|Available\|\|\|` and `BMW-520\|BMW 520i\|350.00\|Rented\|David Lee\|+44 20 7946 0958\|320.00`. `revenue.txt` contains `4226.50`. | ☐ |
| 8.4 | File > Exit | Closes **without** asking (nothing changed since saving). | ☐ |
| 8.5 | Start again. | All data is exactly as saved: 9 vehicles, revenue 4,226.50. | ☐ |
| 8.6 | Edit KIA-777's rate to `110`. Title shows `*`. Click the window's **X**. | The dialog **"Save changes?"** appears with Yes / No / Cancel. | ☐ |
| 8.7 | Click **Cancel**. | The program stays open with the change kept. | ☐ |
| 8.8 | Click X again → **No**. Then restart. | The program closed without saving. After restarting, KIA-777 is back at 105.00. | ☐ |
| 8.9 | Change the rate to `110` again → X → **Yes**. Restart. | Saved and closed. After restarting, KIA-777 is 110.00. | ☐ |

## 9. Loading: validation of every line

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 9.1 | Close the program. Copy `CarRentalSystem\data\vehicles_with_errors.txt` over `x64\Debug\vehicles.txt`. Start the program. | **One** warning dialog: *"10 entries in the data files could not be loaded and were skipped:"*. It lists line 2 (duplicate plate ABC-123), line 3 (invalid plate "BAD PLATE"), line 4 (rate -20.00), line 5 (rate 99.999, more than 2 decimals), line 6 (Rented without renter information), line 7 (expected 7 fields, found 3), line 8 (empty model), line 9 (invalid telephone), line 10 (rate 0), line 11 (unknown status "Lost"). | ☐ |
| 9.2 | Click OK. | The fleet contains only **ABC-123** and **NEW-009** (the valid lines). | ☐ |
| 9.3 | Close the program. There is no save prompt, because loading is not a change. Replace the content of `revenue.txt` with the text `abc`. Start the program. | Warning: *"revenue.txt skipped: "abc" is not a valid amount - cumulative revenue starts at 0.00."* Revenue shows 0.00. | ☐ |
| 9.4 | Close. **Delete** `vehicles.txt`, `revenue.txt` and `history.txt`. Start the program. | No error. Empty fleet, revenue 0.00, empty rental history (missing files = empty data). | ☐ |
| 9.5 | Close. Build again (the sample data is copied back) to restore the starting data. | — | ☐ |

## 10. Keyboard and usability

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 10.1 | Press **Ctrl+Tab** / **Ctrl+Shift+Tab** | Switches to the next / previous tab. | ☐ |
| 10.2 | Press **Tab** repeatedly on a tab page. | The focus moves list → buttons → tab headers in a logical order. | ☐ |
| 10.3 | Press **Alt+A** on the Fleet tab. | Opens Add Vehicle. In the dialog, **Alt+M** jumps to Model and **Alt+D** to Daily rate. | ☐ |
| 10.4 | In any dialog, press **Enter** / **Esc** | Enter = OK (with validation). Esc = Cancel. | ☐ |
| 10.5 | **Ctrl+F** | Jumps to the Search tab and the search box. | ☐ |
| 10.6 | Move the window to a monitor with a different scaling (or change the Windows scale setting). | Text stays sharp, and fonts and layout rescale (Per-Monitor V2 DPI). | ☐ |
| 10.7 | **Help > About** | Version, data folder and keyboard shortcuts. | ☐ |

## 11. Business-rule self-test (summary for the instructor)

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 11.1 | **Help > Run Business Rule Self-Test** | *"36 of 36 business-rule checks passed."* It lists `[PASS]` for every rule: duplicate plate, invalid plate, empty/`\|` model, invalid rates, empty name, invalid phones, renting a rented vehicle, editing a rented vehicle, returning an available vehicle, invalid days (0, -2, 2.5, abc, empty, 99999), the correct charge (140.50 × 3 = 421.50), the rental history (archiving, search, invalid date rejected), exact money arithmetic, and the save/load round trip through `IDataStore&` (polymorphism). It runs on a temporary agency, so your real data is unchanged (the title does not get a `*`). | ☐ |

## 12. Rental history

Start from fresh sample data (step 9.5): 4 completed rentals in `history.txt`.

| # | Action | Expected result | ☐ |
|---|---|---|---|
| 12.1 | Click the **Rental History** tab. | 4 records (receipts 1–4) with columns No., Returned, Plate, Model, Renter, Telephone, Rate, Days, Charge. Summary: **"4 completed rentals \| Total charged: 3,250.00"**, the same as the cumulative revenue. | ☐ |
| 12.2 | Type `emma` in **Search history**. | Only receipt 1 (ABC-123, Emma Wilson, 150.00 × 4 = 600.00). Summary: "1 record matches "emma" \| Total charged: 600.00". | ☐ |
| 12.3 | Type `civic`. | Only receipt 2 (HON-111, Liam Chen, 700.00). The search also matches plate and telephone, for example `0918` finds receipt 4. | ☐ |
| 12.4 | Clear the search. Click the **Charge** header twice. | Sorted by charge, highest first: receipt 3 (1,050.00) is at the top. | ☐ |
| 12.5 | **Double-click** receipt 3 (or select it and press Enter). | The receipt is shown again: "Receipt no. 3 - 2026-10-20 12:40", BMW-520, Sofia Rossi, 350.00 x 3 = 1,050.00. | ☐ |
| 12.6 | Rentals & Returns: return **HON-222** for `3` days. | The receipt now starts with **"Receipt no. 5 - <today's date and time>"**. Rental History shows 5 records and "Total charged: 3,686.50", matching the new revenue. | ☐ |
| 12.7 | **Ctrl+S**, close and restart. Open `x64\Debug\history.txt` in Notepad. | The history still has 5 records. The file has a new line: `5\|<date>\|HON-222\|Honda Civic\|Maria Garcia\|0912-345-678\|145.50\|3\|436.50`. | ☐ |
| 12.8 | Close the program. In `history.txt`, change the last value of line 5 from `436.50` to `999.00`. Start the program. | Warning: *"history.txt line 5 skipped: the charge 999.00 does not equal agreed rate x days (436.50)."* The history shows 4 records. Change the value back to `436.50` afterwards. | ☐ |
