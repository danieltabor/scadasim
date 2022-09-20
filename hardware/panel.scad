screen_width = 98;
screen_height = 40;
screen_depth = 7-1.5;
screen_board_width = screen_width;
screen_board_height = 60;
screen_mount_hole_d = 3;
screen_mount_height = 4;

serial_port_width = 15;
serial_port_height = 7;
serial_board_width = 32;
serial_board_height = 14;
serial_mount_hole_d = 5;

micro_width = 56;
micro_height = 25;
micro_mount_len = 3;
micro_mount_thick = 2;
micro_mount_height = 4;

subpanel_buffer = 2;
panel_thickness = 2;
screen_panel_width = screen_board_width+2*subpanel_buffer;
screen_panel_height = screen_board_height+2*subpanel_buffer;
serial_panel_width = serial_board_width+2*subpanel_buffer;;
serial_panel_height = serial_board_height+2*subpanel_buffer;
micro_panel_width = micro_width + 2*micro_mount_thick + 2*subpanel_buffer;
micro_panel_height = micro_height + 2*micro_mount_thick + 2*subpanel_buffer;


panel_mount_hole_d = 6;
panel_mount_buffer = 20;
panel_center_width = screen_panel_width;
panel_center_height = screen_panel_height + micro_panel_height + serial_panel_height;
panel_width = panel_center_width + 2*panel_mount_buffer;
panel_height = panel_center_height + 2*panel_mount_buffer;


module screen_panel() {
    panel_buffer = subpanel_buffer;
    panel_width = screen_panel_width;
    panel_height = screen_panel_height;
    mount_d = screen_mount_hole_d+2;
    mount_offset = screen_mount_hole_d/2+panel_buffer+1;
    union() {
        difference() {
            cube([panel_width,panel_thickness,panel_height]);
            translate([(panel_width-(screen_width+1))/2,-1,(panel_height-(screen_height+1))/2]) {
                cube([screen_width+1,panel_thickness+2,screen_height+1]);
            }
        }
        translate([mount_offset,0,mount_offset])
            rotate([-90,0,0]) {
                cylinder(panel_thickness+screen_mount_height,d=screen_mount_hole_d,$fn=20);
            }
        translate([panel_width-mount_offset,0,mount_offset])
            rotate([-90,0,0]) {
                cylinder(panel_thickness+screen_mount_height,d=screen_mount_hole_d,$fn=20);
            }
        translate([mount_offset,0,panel_height-mount_offset])
            rotate([-90,0,0]) {
                cylinder(panel_thickness+screen_mount_height,d=screen_mount_hole_d,$fn=20);
            }
        translate([panel_width-mount_offset,0,panel_height-mount_offset])
            rotate([-90,0,0]) {
                cylinder(panel_thickness+screen_mount_height,d=screen_mount_hole_d,$fn=20);
            } 
        }
}

module serial_panel() {
    panel_buffer = subpanel_buffer;
    panel_width = serial_panel_width;
    panel_height = serial_panel_height;
    mount_offset = serial_mount_hole_d/2+panel_buffer;
    difference() {
        cube([panel_width,panel_thickness,panel_height]);
        translate([panel_buffer,1,panel_buffer])
            cube([serial_board_width,panel_thickness,serial_board_height]);
        translate([(panel_width-serial_port_width)/2,-1,(panel_height-serial_port_height)/2])
            cube([serial_port_width+1,panel_thickness+2,serial_port_height+1]);
        translate([mount_offset,-1,panel_height/2])
            rotate([-90,0,0]) {
                cylinder(panel_thickness+2,d=serial_mount_hole_d,$fn=20);
            }
        translate([panel_width-mount_offset,-1,panel_height/2])
            rotate([-90,0,0]) {
                cylinder(panel_thickness+2,d=serial_mount_hole_d,$fn=20);
            }
    }
}

module micro_panel() {
    panel_buffer = subpanel_buffer;
    panel_width = micro_panel_width;
    panel_height = micro_panel_height;
    union() {
        cube([panel_width,panel_thickness,panel_height]);
        translate([panel_buffer,0,panel_buffer])
        difference() {
            cube([micro_width + 2*micro_mount_thick, panel_thickness+micro_mount_height, micro_height + 2*micro_mount_thick]);
            translate([0,panel_thickness,0])
            union() {
               translate([micro_mount_thick+micro_mount_len,0,-1])
                cube([micro_width-2*micro_mount_len, micro_mount_height+1, micro_height + 2*micro_mount_thick+2]);
                translate([-1,0,micro_mount_thick+micro_mount_len])
                    cube([micro_width + 2*micro_mount_thick+2, micro_mount_height+1, micro_height-2*micro_mount_len]);
                translate([micro_mount_thick,0,micro_mount_thick])
                    cube([micro_width,micro_mount_height+1,micro_height]);
            }
        }
    }
}

module label(text) {
    rotate([90,0,0])
        linear_extrude(2)
            text(text,size=5,valign="bottom",halign="center",font="courier");
}

module panel() {
    mount_offset = panel_mount_hole_d/2+(panel_mount_buffer-panel_mount_hole_d)/2;
    difference() {
        union() {  
            difference() {
                cube([panel_width,panel_thickness,panel_height]);
                translate([(panel_width-panel_center_width)/2,-1,(panel_height-panel_center_height)/2])
                    cube([panel_center_width,panel_thickness+2,panel_center_height]);
                translate([mount_offset,-1,mount_offset])
                    rotate([-90,0,0])
                        cylinder(panel_thickness+2,d=panel_mount_hole_d,$fn=20);
                translate([panel_width-mount_offset,-1,mount_offset])
                    rotate([-90,0,0])
                        cylinder(panel_thickness+2,d=panel_mount_hole_d,$fn=20);
                translate([mount_offset,-1,panel_height-mount_offset])
                    rotate([-90,0,0])
                        cylinder(panel_thickness+2,d=panel_mount_hole_d,$fn=20);
                translate([panel_width-mount_offset,-1,panel_height-mount_offset])
                    rotate([-90,0,0])
                        cylinder(panel_thickness+2,d=panel_mount_hole_d,$fn=20);
            }
            translate([panel_mount_buffer,0,panel_mount_buffer])
            union() {
                translate([0,0,0])
                    serial_panel();
                translate([panel_center_width-serial_panel_width,0,0])
                    serial_panel();
                translate([serial_panel_width,0,0])
                    cube([panel_center_width-2*serial_panel_width,panel_thickness,serial_panel_height]);
                translate([(panel_center_width-micro_panel_width)/2,0,serial_panel_height])
                    micro_panel();
                translate([0,0,serial_panel_height])
                    cube([panel_center_width,panel_thickness,micro_panel_height]);
                translate([0,0,serial_panel_height+micro_panel_height])
                    screen_panel();
            }
        }
        translate([panel_mount_buffer+serial_panel_width/2+1,1,panel_mount_buffer+serial_panel_height+5])
            label("Data");
        translate([panel_width-panel_mount_buffer-serial_panel_width/2+1,1,panel_mount_buffer+serial_panel_height+5])
            label("Console");
    }
}

/*

*/

//serial_panel();
panel();
