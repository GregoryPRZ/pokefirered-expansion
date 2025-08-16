#!/usr/bin/env python3
"""
Fix PNG file widths to be multiples of 8 pixels (required for GBA graphics).
This script will pad images with transparent pixels to reach the next multiple of 8.
"""

from PIL import Image
import os

def fix_width_to_multiple_of_8(image_path):
    """Fix the width of a PNG file to be a multiple of 8."""
    try:
        with Image.open(image_path) as img:
            width, height = img.size
            
            if width % 8 == 0:
                print(f"✓ {image_path}: {width}x{height} - Already OK")
                return True
            
            # Calculate the new width (next multiple of 8)
            new_width = ((width + 7) // 8) * 8
            
            print(f"Processing: {image_path}")
            print(f"  Current size: {width}x{height}")
            print(f"  New width: {new_width} (padded by {new_width - width} pixels)")
            
            # Create a new image with the correct width
            # For palette mode images, we need to preserve the palette
            if img.mode == 'P':
                # Create new image in palette mode
                new_img = Image.new('P', (new_width, height))
                new_img.putpalette(img.getpalette())
                
                # Paste the original image at the left side
                new_img.paste(img, (0, 0))
                
                # The rest will be filled with color index 0 (usually transparent or background)
            else:
                # For other modes, create with transparent background
                new_img = Image.new('RGBA', (new_width, height), (0, 0, 0, 0))
                new_img.paste(img, (0, 0))
            
            # Save the fixed image
            new_img.save(image_path, 'PNG')
            print(f"  ✓ Saved with new dimensions: {new_width}x{height}")
            return True
            
    except Exception as e:
        print(f"  ✗ Error processing {image_path}: {e}")
        return False

def main():
    # Files that need width fixing based on our scan
    files_to_fix = [
        'graphics/object_events/pics/people/cooltrainer_f.png',      # 158 -> 160
        'graphics/object_events/pics/people/woman_1.png',           # 138 -> 144  
        'graphics/object_events/pics/people/woman_3.png',           # 141 -> 144
        'graphics/object_events/pics/people/schoolkid_f.png',       # 179 -> 184
        'graphics/object_events/pics/people/schoolkid_m.png',       # 145 -> 152
        'graphics/object_events/pics/people/juggler.png',           # 178 -> 184
        'graphics/object_events/pics/people/bird_keeper.png'        # 171 -> 176
    ]
    
    print("Fixing PNG file widths to be multiples of 8...")
    print("=" * 60)
    
    success_count = 0
    error_count = 0
    
    for file_path in files_to_fix:
        if os.path.exists(file_path):
            if fix_width_to_multiple_of_8(file_path):
                success_count += 1
            else:
                error_count += 1
        else:
            print(f"✗ File not found: {file_path}")
            error_count += 1
        print()
    
    print("=" * 60)
    print(f"Width fixing complete!")
    print(f"Successfully fixed: {success_count} files")
    print(f"Errors: {error_count} files")
    
    if error_count == 0:
        print("\n✓ All files fixed! Widths are now multiples of 8.")
    else:
        print(f"\n⚠ {error_count} files had errors.")

if __name__ == "__main__":
    main()
