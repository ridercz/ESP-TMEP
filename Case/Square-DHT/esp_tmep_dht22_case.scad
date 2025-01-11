include <A2D.scad>; // https://github.com/ridercz/A2D
assert(a2d_required([1, 6, 2]), "Please upgrade A2D library to version 1.6.2 or higher.");

/* Configuration *****************************************************************************************************/

/* [Board size] */
dht_board = [21.5, 38.5, 8];
esp_board = [25.5, 34.5, 5];

/* [USB connector] */
usb_size = [11, 6.5];
usb_radius = 2;
usb_zoff = 3;

/* [Grill] */
grill_size = 3;
grill_spacing = 1.67;

/* [Construction] */
wall_thickness = 1.67;
bottom_thickness = 1.2;
top_thickness = 1.2;
rail_height = 3;
rail_width = 2;
divider_hole = 10;

/* [Hidden] */
$fn = 32;
$fudge = 1;
sig_text = ["ESP-TMEP-DHT22", "R2 by RIDER.CZ"];
sig_font = "Arial:style=Bold";
sig_size = 5;
sig_depth = .4;
outer_size = [
    max(dht_board.x, esp_board.x) + 2 * wall_thickness,
    dht_board.y + esp_board.y + 3 * wall_thickness,
    rail_height + max(dht_board.z, esp_board.z) + top_thickness + bottom_thickness
];
echo(outer_size = outer_size);
dht_rail_width = rail_width + (max(dht_board.x, esp_board.x) - dht_board.x) / 2 + wall_thickness;
esp_rail_width = rail_width + (max(dht_board.x, esp_board.x) - esp_board.x) / 2 + wall_thickness;

/* Render ***********************************************************************************************************/

part_lid();
translate([outer_size.x * 1.1, 0]) part_bottom();

/* Modules **********************************************************************************************************/

module part_lid() {
    linear_extrude(height = bottom_thickness) difference() {
        square([outer_size.x, outer_size.y]);
        translate([wall_thickness * 2.5, wall_thickness * 2.5]) grill_mask_square_auto([outer_size.x - 5 * wall_thickness, outer_size.y - 5 * wall_thickness], grill_size, grill_spacing);
    }
    translate([wall_thickness, wall_thickness]) linear_extrude(height = bottom_thickness * 2)  h_square([outer_size.x - wall_thickness * 2, outer_size.y - wall_thickness * 2], thickness = -wall_thickness);
}

module part_bottom() {
    difference() {
        // Main body
        union() {
            // Bottom
            cube([outer_size.x, outer_size.y, bottom_thickness]);

            // Sides
            linear_extrude(height = outer_size.z - top_thickness) h_square([outer_size.x, outer_size.y], thickness = -wall_thickness);

            // Divider
            translate([0, dht_board.y + wall_thickness]) cube([outer_size.x, wall_thickness, outer_size.z - 2.5 * top_thickness]);

            // DHT rails
            cube([dht_rail_width, dht_board.y + wall_thickness, rail_height + bottom_thickness]);
            translate([outer_size.x - dht_rail_width, 0])cube([dht_rail_width, dht_board.y + wall_thickness, rail_height + bottom_thickness]);

            // ESP rails
            translate([0, dht_board.y + 2 * wall_thickness]) cube([esp_rail_width, esp_board.y + wall_thickness, rail_height + bottom_thickness]);
            translate([outer_size.x - esp_rail_width, dht_board.y + 2 * wall_thickness])cube([esp_rail_width, esp_board.y + wall_thickness, rail_height + bottom_thickness]);

            // Size adjustment for DHT
            if(esp_board.x > dht_board.x) {
                cube([dht_rail_width - rail_width, dht_board.y + wall_thickness, outer_size.z - 2.5 * top_thickness]);
                translate([outer_size.x - dht_rail_width + rail_width, 0])cube([dht_rail_width - rail_width, dht_board.y + wall_thickness, outer_size.z - 2.5 * top_thickness]);
            }

            // Size adjustment for ESP
            if(esp_board.x < dht_board.x) {
                cube([esp_rail_width - rail_width, esp_board.y + wall_thickness, outer_size.z - 2.5 * top_thickness]);
                translate([outer_size.x - esp_rail_width + rail_width, 0])cube([esp_rail_width - rail_width, esp_board.y + wall_thickness, outer_size.z - 2.5 * top_thickness]);
            }
        }

        // Divider hole
        translate([(outer_size.x - divider_hole) / 2, dht_board.y + wall_thickness - $fudge, bottom_thickness + rail_height]) cube([divider_hole, wall_thickness + 2 * $fudge, outer_size.z]);

        // USB connector hole
        translate([(outer_size.x - usb_size.x) / 2, outer_size.y + $fudge, bottom_thickness + rail_height + usb_zoff - usb_size.y / 2]) rotate([90,0,0]) linear_extrude(height = wall_thickness + 2 * $fudge) r_square(usb_size, usb_radius);

        // Signature text
        translate([outer_size.x / 2, outer_size.y / 2, -$fudge]) linear_extrude(height = sig_depth + $fudge) rotate([180, 0, 90]) multiline_text(sig_text, size = sig_size, font = sig_font, halign = "center", valign = "center", $fn = 32);
    }
}