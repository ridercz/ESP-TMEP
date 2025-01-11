// Based on Wemos D1 mini box (http://www.thingiverse.com/thing:1995963) 
// by matsekberg is licensed under the Creative Commons - Attribution license

/* [Board size] */
board_x = 26.0;
board_y = 34.5;
board_y_tolerance = .5;
board_y_cucutout = 18;
board_corner = 2.0;

/* [USB connector hole] */
usb_x = 13;
usb_z = 7;
usb_x_pos = 7;
usb_z_pos = -1;

/* [Mount holes] */
mount_hole_diameter = 4;
mount_hole_spacing = 40;
mount_hole_padding = 5;

/* [Side holes] */
top_hole_diameter = 6.5;
left_hole_diameter = 0;
left_hole_y = 45;
right_hole_diameter = 4.5;
right_hole_y = 40;

/* [Miscellaneous] */
bottom_height = 24;
extra_room_y = 15;
wall_thickness = 1.68;
stud_height = 3;
stud_width = 2;
tolerance = 0;

/* [Lid text] */
lid_text_1 = "ESP";
lid_text_2 = "TMEP";
lid_text_font = "Arial:style=Bold";
lid_text_size = 6.5;
lid_text_extr = .4;

/* [Hidden] */
box_size = [
    board_x + 2 * wall_thickness, 
    board_y + extra_room_y + 2 * wall_thickness, 
    bottom_height + wall_thickness
];
part_spacing = 3;
$fudge = 1;

echo(box_size = box_size);

/* Main render ******************************************************************************************************/

// Lid with text
if(lid_text_size > 0) translate([0, box_size.y + part_spacing]) {
    difference() {
        part_lid();
        translate(v = [0, 0, -$fudge]) linear_extrude(height = lid_text_extr + $fudge) {
            translate(v = [box_size.x / 2, box_size.y / 2 + lid_text_size * .1]) mirror(v = [1,0,0]) text(text = lid_text_1, size = lid_text_size, font = lid_text_font, halign = "center", valign = "bottom");
            translate(v = [box_size.x / 2, box_size.y / 2 - lid_text_size * .1]) mirror(v = [1,0,0]) text(text = lid_text_2, size = lid_text_size, font = lid_text_font, halign = "center", valign = "top");
        }
    }
}

// Box with holes
difference() {
    part_box();

    // Top hole
    if(top_hole_diameter > 0) translate([box_size.x / 2, box_size.y - wall_thickness - $fudge, (box_size.z + wall_thickness) / 2]) rotate([-90, 0, 0]) cylinder(d = top_hole_diameter, h = box_size.z + wall_thickness, $fn = 32);

    // Left hole
    if(left_hole_diameter > 0) translate([-$fudge, left_hole_y, (box_size.z + wall_thickness) / 2]) rotate([0, 90, 0]) cylinder(d = left_hole_diameter, h = wall_thickness + 2 * $fudge, $fn = 32);

    // Right hole
    if(right_hole_diameter > 0) translate([box_size.x - wall_thickness - $fudge, right_hole_y, (box_size.z + wall_thickness) / 2]) rotate([0, 90, 0]) cylinder(d = right_hole_diameter, h = wall_thickness + 2 * $fudge, $fn = 32);
}

/* Parts ***********************************************************************************************************/

module part_lid() {
    translate([wall_thickness, wall_thickness]) {
        linear_extrude(height = wall_thickness) perimeter(wall_thickness, extra_room_y);
        translate([0, 0, wall_thickness]) linear_extrude(height = wall_thickness) difference() {
            perimeter(-tolerance, extra_room_y);
            perimeter(-wall_thickness, extra_room_y);
        }
    }
}

module part_box() {
    translate([wall_thickness, wall_thickness]) {
        // Main shell
        difference() {
            union() {
                // Bottom plate
                linear_extrude(height = wall_thickness) difference() {
                    hull() {
                        perimeter(wall_thickness, extra_room_y);
                        if(mount_hole_diameter > 0) for(x = [(box_size.x - mount_hole_spacing) / 2 - wall_thickness, (box_size.x + mount_hole_spacing) / 2 - wall_thickness]) translate([x, box_size.y / 2]) circle(d = mount_hole_diameter + 2 * mount_hole_padding, $fn = 32);
                    }
                    if(mount_hole_diameter > 0) for(x = [(box_size.x - mount_hole_spacing) / 2 - wall_thickness, (box_size.x + mount_hole_spacing) / 2 - wall_thickness]) translate([x, box_size.y / 2]) circle(d = mount_hole_diameter, $fn = 32);
                }

                // Outside walls
                translate([0, 0, wall_thickness])  linear_extrude(height = bottom_height - wall_thickness)difference() {
                    perimeter(wall_thickness, extra_room_y);
                    perimeter(0, extra_room_y);
                }
            }


            // USB port hole
            translate([usb_x_pos, -1.5 * wall_thickness, usb_z_pos + wall_thickness + stud_height]) cube(size = [usb_x, wall_thickness * 2, usb_z]);
        }
        
        // Board studs
        translate([0, 0, wall_thickness]) cube(size = [board_x, stud_width, stud_height]);
        difference() {
            translate([0, board_y - stud_width + board_y_tolerance, wall_thickness]) cube(size = [board_x, stud_width, stud_height]);
            translate([(board_x - board_y_cucutout) / 2, board_y - stud_width + board_y_tolerance - $fudge, wall_thickness + stud_height / 2]) cube(size = [board_y_cucutout, stud_width + $fudge, stud_height]);
        }

        // Board support
        translate([0, board_y + board_y_tolerance, wall_thickness]) cube(size = [board_x, stud_width, stud_height * 1.5]);
    }
}

/* Helpers **********************************************************************************************************/

module perimeter(expand, extra_y) {
    polygon(points=[
        [-expand, -expand], 
        [-expand, board_y - board_corner + expand + extra_y],
        [board_corner - expand, board_y + expand + extra_y],
        [board_x - board_corner + expand, board_y + expand + extra_y],
        [board_x + expand, board_y - board_corner + expand + extra_y],
        [board_x + expand, -expand]
    ]);
}
