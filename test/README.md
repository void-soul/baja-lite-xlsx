# Test Files

## Creating a Sample Excel File

To run the tests, you need to create a `sample.xlsx` file in this directory with the following content:

### Method 1: Using Microsoft Excel or LibreOffice Calc

1. Create a new Excel file
2. In **Sheet1**, add some sample data:
   ```
   A1: Name      B1: Age    C1: City
   A2: Alice     B2: 30     C2: New York
   A3: Bob       B3: 25     C3: London
   A4: Charlie   B4: 35     C4: Tokyo
   ```

3. Insert some images:
   - Go to Insert → Picture
   - Add 1-2 images to the sheet
   - Resize and position them over cells

4. Save the file as `sample.xlsx` in this `test/` directory

### Method 2: Using a Pre-made Template

Download a sample Excel file with images from:
- [Download Sample XLSX](https://file-examples.com/index.php/sample-documents-download/sample-xls-download/)

Or create your own using any spreadsheet software.

### Minimum Requirements

The test file should have:
- ✓ At least 1 sheet with data
- ✓ At least a few rows and columns of data
- ✓ (Optional) 1 or more embedded images

### Running Tests

Once you have `sample.xlsx` in place:

```bash
npm test
```

The test will:
1. Read all sheets and display their names and row counts
2. Extract all images and show their sizes
3. Display image position information
4. Save extracted images to `test/output/` directory

### Expected Output

```
=== Baja-XLSX Native Module Test ===

1. Testing readExcel()...

   Sheets found: 1
   - Sheet 1: "Sheet1" (4 rows)
     First row has 3 columns
     Sample data: ["Name","Age","City"]

   Images found: 2
   - Image 1: image1.png (12345 bytes, type: image/png)
   - Image 2: image2.jpg (23456 bytes, type: image/jpeg)

   Image positions found: 2
   - Position 1: image1.png in sheet "Sheet1"
     From: col 2, row 5
     To: col 4, row 10
   ...

✓ All tests completed successfully!
```


