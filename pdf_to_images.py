import fitz
from pathlib import Path


# ============================================================
# CONFIGURATION
# ============================================================

# Folder containing your scanned PDF files
INPUT_FOLDER = Path("input_pdfs")

# Folder where converted images will be saved
OUTPUT_FOLDER = Path("pdf_images")

# Image resolution
# 200-300 DPI is generally suitable for document vision models.
DPI = 200


# ============================================================
# PDF -> IMAGES
# ============================================================

def pdf_to_images(pdf_path, output_folder, dpi=200):

    # Create output folder for this PDF
    pdf_name = pdf_path.stem
    pdf_output_folder = output_folder / pdf_name
    pdf_output_folder.mkdir(parents=True, exist_ok=True)

    # Open PDF
    document = fitz.open(pdf_path)

    print(f"\nProcessing: {pdf_path.name}")
    print(f"Number of pages: {len(document)}")

    # Calculate zoom from DPI
    zoom = dpi / 72
    matrix = fitz.Matrix(zoom, zoom)

    # Convert every page
    for page_number in range(len(document)):

        page = document[page_number]

        # Render page as image
        pixmap = page.get_pixmap(
            matrix=matrix,
            alpha=False
        )

        # Output filename
        image_path = (
            pdf_output_folder /
            f"page_{page_number + 1:03d}.png"
        )

        # Save image
        pixmap.save(image_path)

        print(f"  Saved: {image_path}")

    document.close()

    print(f"Completed: {pdf_path.name}")


# ============================================================
# PROCESS ALL PDFs
# ============================================================

def main():

    # Create folders if they don't exist
    INPUT_FOLDER.mkdir(exist_ok=True)
    OUTPUT_FOLDER.mkdir(exist_ok=True)

    # Find all PDF files
    pdf_files = list(INPUT_FOLDER.glob("*.pdf"))

    if not pdf_files:
        print("\nNo PDF files found.")
        print(f"Put your PDF files inside: {INPUT_FOLDER.resolve()}")
        return

    print(f"\nFound {len(pdf_files)} PDF file(s).")

    # Process every PDF
    for pdf_file in pdf_files:

        pdf_to_images(
            pdf_file,
            OUTPUT_FOLDER,
            DPI
        )

    print("\n========================================")
    print("ALL PDFs CONVERTED SUCCESSFULLY")
    print("========================================")
    print(f"Images saved in: {OUTPUT_FOLDER.resolve()}")


# ============================================================
# PROGRAM START
# ============================================================

if __name__ == "__main__":
    main()