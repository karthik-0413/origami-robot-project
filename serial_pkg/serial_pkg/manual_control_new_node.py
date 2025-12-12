#!/usr/bin/env python3
import math
import pygame
import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32, Int32MultiArray

# ---------------- Pygame setup ----------------
pygame.init()
screen = pygame.display.set_mode((1200, 900), pygame.RESIZABLE)
clock = pygame.time.Clock()
pygame.display.set_caption("Hinge & Navigation UI")

FONT  = pygame.font.SysFont("consolas", 26)
SMALL = pygame.font.SysFont("consolas", 32)  # bigger text

# ---------------- Tabs ----------------
TAB_HINGE      = "HINGE"
TAB_NAVIGATION = "NAVIGATION"
current_tab    = TAB_HINGE

TAB_HEIGHT = 50
TAB_BG     = (40, 40, 40)
TAB_ACTIVE = (90, 90, 90)
TAB_TEXT   = (255, 255, 255)
TAB_TEXT_FADED_ALPHA = 90

def draw_tabs(surface, active_tab):
    W, _ = surface.get_size()

    hinge_rect = pygame.Rect(0, 0, W//2, TAB_HEIGHT)
    nav_rect   = pygame.Rect(W//2, 0, W//2, TAB_HEIGHT)

    pygame.draw.rect(surface, TAB_ACTIVE if active_tab == TAB_HINGE else TAB_BG, hinge_rect)
    pygame.draw.rect(surface, TAB_ACTIVE if active_tab == TAB_NAVIGATION else TAB_BG, nav_rect)

    def draw_label(text, rect, active):
        label = FONT.render(text, True, TAB_TEXT)
        if not active:
            label.set_alpha(TAB_TEXT_FADED_ALPHA)
        surf = pygame.Surface(label.get_size(), pygame.SRCALPHA)
        surf.blit(label, (0,0))
        surface.blit(
            surf,
            (rect.centerx - label.get_width() / 2,
             rect.centery - label.get_height() / 2),
        )

    draw_label("HINGE", hinge_rect, active_tab == TAB_HINGE)
    draw_label("NAVIGATION", nav_rect, active_tab == TAB_NAVIGATION)

    return hinge_rect, nav_rect


# ---------------- Shared colors ----------------
BG            = (24, 24, 24)
RECT_FILL     = (40, 80, 40)
RECT_OUTLINE  = (120, 200, 120)
RECT_SELECTED_FILL     = (140, 90, 0)
RECT_SELECTED_OUTLINE  = (255, 160, 20)

WAVE_ON  = (255, 80, 80, 255)
WAVE_OFF = (255, 255, 255, 60)

ARROW_ON  = (255, 220, 40, 255)
ARROW_OFF = (255, 255, 255, 80)


# ---------------- Arrow drawing ----------------
def draw_arrow(surface, center, direction, on, scale):
    arrow_surf = pygame.Surface(surface.get_size(), pygame.SRCALPHA)
    color = ARROW_ON if on else ARROW_OFF

    x, y = center
    s    = scale * 0.30
    stem = scale * 0.18
    head = scale * 0.24

    if direction == "up":
        pts=[(x,y-s),(x-head,y),(x-stem/2,y),(x-stem/2,y+s),
             (x+stem/2,y+s),(x+stem/2,y),(x+head,y)]
    elif direction == "down":
        pts=[(x,y+s),(x-head,y),(x-stem/2,y),(x-stem/2,y-s),
             (x+stem/2,y-s),(x+stem/2,y),(x+head,y)]
    elif direction == "left":
        pts=[(x-s,y),(x,y-head),(x,y-stem/2),(x+s,y-stem/2),
             (x+s,y+stem/2),(x,y+stem/2),(x,y+head)]
    elif direction == "right":
        pts=[(x+s,y),(x,y-head),(x,y-stem/2),(x-s,y-stem/2),
             (x-s,y+stem/2),(x,y+stem/2),(x,y+head)]
    else:
        return

    pygame.draw.polygon(arrow_surf, color, pts)
    surface.blit(arrow_surf, (0,0))


# ---------------- ROS2 Node ----------------
class HingeNavUINode(Node):
    def __init__(self):
        super().__init__("hinge_nav_ui")

        # Publishers
        self.drive_pub = self.create_publisher(Int32, "/drive_state", 10)
        self.turn_pub  = self.create_publisher(Int32, "/turn_state", 10)
        self.hinge_drive_pub = self.create_publisher(Int32MultiArray, "/hinge_drive_state", 10)

        # Subscribers
        self.ir_sub = self.create_subscription(
            Int32MultiArray, "/ir_sensors", self.ir_callback, 10
        )
        self.hinge_sub = self.create_subscription(
            Int32MultiArray, "/hinge_angles", self.hinge_callback, 10
        )

        # Navigation state
        self.waves_state = {
            "top": False,
            "right_top": False,
            "right_bottom": False,
            "bottom": False,
            "left_bottom": False,
            "left_top": False,
        }
        self.nav_arrows = {"up": False, "down": False, "left": False, "right": False}

        # Hinge state (angles in DEGREES)
        self.target_angle_left   = 0.0
        self.target_angle_right  = 0.0
        self.current_angle_left  = 0.0
        self.current_angle_right = 0.0
        self.selected_hinge = "left"  # "left" or "right"
        self.hinge_arrows = {"up": False, "down": False}

    # ------ Subscribers ------
    def ir_callback(self, msg: Int32MultiArray):
        # /ir_sensors: size 6
        # 0-top, 1-topright, 2-bottomright, 3-bottom, 4-bottomleft, 5-topleft
        data = list(msg.data)
        if len(data) >= 6:
            self.waves_state["top"]         = bool(data[0])
            self.waves_state["right_top"]   = bool(data[1])
            self.waves_state["left_top"]    = bool(data[2])
            self.waves_state["bottom"]      = bool(data[3])
            self.waves_state["left_bottom"] = bool(data[4])
            self.waves_state["right_bottom"]= bool(data[5])

    def hinge_callback(self, msg: Int32MultiArray):
        # /hinge_angles: size 2, ints in degrees
        data = list(msg.data)
        if len(data) >= 2:
            left_deg  = float(data[0])
            right_deg = float(data[1])
            # clamp to [-90, 90]
            self.target_angle_left  = max(-90.0, min(90.0, left_deg))
            self.target_angle_right = max(-90.0, min(90.0, right_deg))

    # ------ Publishers ------
    def publish_drive_and_turn(self, active_tab: str):
        # Only publish in NAVIGATION tab
        if active_tab != TAB_NAVIGATION:
            return

        # DRIVE STATE from up/down nav arrows
        up   = self.nav_arrows["up"]
        down = self.nav_arrows["down"]
        if up and not down:
            drive_val = 1
        elif down and not up:
            drive_val = -1
        else:
            drive_val = 0

        # TURN STATE from left/right nav arrows
        left  = self.nav_arrows["left"]
        right = self.nav_arrows["right"]
        if left and not right:
            turn_val = -1
        elif right and not left:
            turn_val = 1
        else:
            turn_val = 0

        self.drive_pub.publish(Int32(data=drive_val))
        self.turn_pub.publish(Int32(data=turn_val))


    def publish_hinge_drive(self, active_tab: str):
        # Only publish in HINGE tab
        if active_tab != TAB_HINGE:
            return

        # HINGE DRIVE STATE from up/down hinge arrows
        up   = self.hinge_arrows["up"]
        down = self.hinge_arrows["down"]

        if up and not down:
            drive_val = 1
        elif down and not up:
            drive_val = -1
        else:
            drive_val = 0

        left_drive  = drive_val if self.selected_hinge == "left" else 0
        right_drive = drive_val if self.selected_hinge == "right" else 0

        msg = Int32MultiArray()
        msg.data = [left_drive, right_drive]
        self.hinge_drive_pub.publish(msg)

# ---------------- Hinge drawing ----------------
def draw_bar_from_hinge(surface, hinge, angle_deg, length, thickness,
                        fill, outline, side):
    """
    Draw a rectangle (bar) that pivots exactly at `hinge`.
    side: 'left' or 'right' -> which side of the hinge the bar extends to
    angle in DEGREES.
    """
    angle_rad = math.radians(angle_deg)

    dirx = math.cos(angle_rad)
    diry = math.sin(angle_rad)

    if side == "left":
        dirx, diry = -dirx, -diry  # extend left when 0°

    perpx, perpy = -diry, dirx
    half_t = thickness / 2.0
    hx, hy = hinge

    p0 = (hx + perpx*half_t, hy + perpy*half_t)
    p1 = (hx - perpx*half_t, hy - perpy*half_t)
    farx = hx + dirx*length
    fary = hy + diry*length
    p2 = (farx - perpx*half_t, fary - perpy*half_t)
    p3 = (farx + perpx*half_t, fary + perpy*half_t)

    pts_int = [(int(x), int(y)) for x,y in (p0,p1,p2,p3)]
    pygame.draw.polygon(surface, fill, pts_int)
    pygame.draw.polygon(surface, outline, pts_int, max(int(thickness*0.12), 3))


def draw_hinge_view(surface, node: HingeNavUINode):
    W, H = surface.get_size()
    scale = min(W, H) * 0.16

    # move hinge system up so bar doesn't overlap text when vertical
    cy = TAB_HEIGHT + H * 0.32
    cx = W * 0.50

    length = W * 0.22
    thick  = H * 0.06
    gap    = W * 0.06

    # Middle bar (fixed)
    mid_rect = pygame.Rect(cx - length/2, cy - thick/2, length, thick)
    pygame.draw.rect(surface, RECT_FILL, mid_rect)
    pygame.draw.rect(surface, RECT_OUTLINE, mid_rect, max(int(thick*0.12), 3))

    # Joints
    left_joint  = (mid_rect.left  - gap/2, cy)
    right_joint = (mid_rect.right + gap/2, cy)

    def get_colors(which):
        if which == node.selected_hinge:
            return RECT_SELECTED_FILL, RECT_SELECTED_OUTLINE
        return RECT_FILL, RECT_OUTLINE

    # Left bar
    fill, outline = get_colors("left")
    draw_bar_from_hinge(
        surface,
        left_joint,
        node.current_angle_left,
        length,
        thick,
        fill,
        outline,
        side="left",
    )

    # Right bar
    fill, outline = get_colors("right")
    draw_bar_from_hinge(
        surface,
        right_joint,
        node.current_angle_right,
        length,
        thick,
        fill,
        outline,
        side="right",
    )

    # Arrows under the middle rect, slightly to the right
    arrow_center_y = mid_rect.bottom + thick * 3.2
    arrow_center_x = cx + length * 0.15
    arrow_gap = scale * 0.60

    # prevent arrows from toggling if angle out of [-90, 90]
    if node.selected_hinge == "left":
        ang = node.current_angle_left
    else:
        ang = node.current_angle_right

    MIN_ANG = -90.0
    MAX_ANG =  90.0
    EPS     =  0.5

    up_allowed   = ang < MAX_ANG - EPS
    down_allowed = ang > MIN_ANG + EPS

    up_on   = node.hinge_arrows["up"]   and up_allowed
    down_on = node.hinge_arrows["down"] and down_allowed

    draw_arrow(surface, (arrow_center_x, arrow_center_y - arrow_gap),
               "up", up_on, scale)
    draw_arrow(surface, (arrow_center_x, arrow_center_y + arrow_gap),
               "down", down_on, scale)

    # --- Text info (3 lines) ---
    selected = node.selected_hinge.upper()
    current_angle = ang

    line1 = SMALL.render(f"Selected Hinge: {selected}", True, (230,230,230))
    line2 = SMALL.render(f"Current Angle: {current_angle:.1f}°", True, (230,230,230))
    # "Selected Angle" = same as current for now
    line3 = SMALL.render(f"Selected Angle: {current_angle:.1f}°", True, (230,230,230))

    text_x = arrow_center_x - scale * 3.4
    base_y = arrow_center_y - line1.get_height()*1.6

    surface.blit(line1, (text_x, base_y))
    surface.blit(line2, (text_x, base_y + line1.get_height()*1.1))
    surface.blit(line3, (text_x, base_y + line1.get_height()*2.2))

    surface.blit(
        SMALL.render("Hinge View", True, (230,230,230)),
        (20, TAB_HEIGHT + 10),
    )


# ---------------- Navigation drawing ----------------
def draw_wave(surface, center, orientation, on, scale):
    r1 = scale * 0.20
    r2 = scale * 0.30
    width = max(int(scale * 0.025), 3)
    color = WAVE_ON if on else WAVE_OFF

    wave_surf = pygame.Surface(surface.get_size(), pygame.SRCALPHA)

    if orientation == "top":
        start,end = math.radians(20),math.radians(160)
    elif orientation=="bottom":
        start,end = math.radians(200),math.radians(340)
    elif orientation=="left":
        start,end = math.radians(110),math.radians(250)
    elif orientation=="right":
        start,end = math.radians(290),math.radians(430)

    for r in (r1,r2):
        rect = pygame.Rect(0,0,r*2,r*2)
        rect.center = center
        pygame.draw.arc(wave_surf,color,rect,start,end,width)

    surface.blit(wave_surf,(0,0))


def draw_navigation_view(surface, node: HingeNavUINode):
    W,H = surface.get_size()
    scale = min(W,H) * 0.17
    rect_size = scale

    cx = W * 0.30
    cy1 = TAB_HEIGHT + H * 0.26
    cy2 = TAB_HEIGHT + H * 0.50
    cy3 = TAB_HEIGHT + H * 0.74

    for cy in (cy1,cy2,cy3):
        rect = pygame.Rect(cx-rect_size/2, cy-rect_size/2, rect_size, rect_size)
        pygame.draw.rect(surface, RECT_FILL, rect)
        pygame.draw.rect(surface, RECT_OUTLINE, rect, max(int(scale*0.03),3))

    offset = scale * 0.40

    draw_wave(surface,(cx, cy1-rect_size/2-offset),"top",
              node.waves_state["top"],scale)
    draw_wave(surface,(cx, cy3+rect_size/2+offset),"bottom",
              node.waves_state["bottom"],scale)
    draw_wave(surface,(cx+rect_size/2+offset, cy1),"right",
              node.waves_state["right_top"],scale)
    draw_wave(surface,(cx+rect_size/2+offset, cy3),"right",
              node.waves_state["right_bottom"],scale)
    draw_wave(surface,(cx-rect_size/2-offset, cy1),"left",
              node.waves_state["left_top"],scale)
    draw_wave(surface,(cx-rect_size/2-offset, cy3),"left",
              node.waves_state["left_bottom"],scale)

    acx = W * 0.72
    acy = TAB_HEIGHT + H * 0.50
    aoff = scale * 0.60

    draw_arrow(surface,(acx,acy-aoff),"up",   node.nav_arrows["up"],scale)
    draw_arrow(surface,(acx,acy+aoff),"down", node.nav_arrows["down"],scale)
    draw_arrow(surface,(acx-aoff,acy),"left", node.nav_arrows["left"],scale)
    draw_arrow(surface,(acx+aoff,acy),"right",node.nav_arrows["right"],scale)

    surface.blit(
        SMALL.render("Navigation View", True, (230,230,230)),
        (20, TAB_HEIGHT + 10),
    )


# ---------------- Main loop ----------------
def main():
    global current_tab

    rclpy.init()
    node = HingeNavUINode()

    running = True
    keys_down = set()

    while running:
        # ---- ROS spin (non-blocking) ----
        rclpy.spin_once(node, timeout_sec=0.0)

        # ---- Pygame events ----
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.MOUSEBUTTONDOWN:
                mx,my = event.pos
                W,_ = screen.get_size()
                hinge_rect = pygame.Rect(0,0,W//2,TAB_HEIGHT)
                nav_rect   = pygame.Rect(W//2,0,W//2,TAB_HEIGHT)
                if hinge_rect.collidepoint(mx,my):
                    current_tab = TAB_HINGE
                elif nav_rect.collidepoint(mx,my):
                    current_tab = TAB_NAVIGATION

            elif event.type == pygame.KEYDOWN:
                keys_down.add(event.key)
                if event.key == pygame.K_TAB:
                    current_tab = (
                        TAB_NAVIGATION if current_tab == TAB_HINGE else TAB_HINGE
                    )

            elif event.type == pygame.KEYUP:
                if event.key in keys_down:
                    keys_down.remove(event.key)

        # ---- Handle input by tab ----

        # HINGE TAB: only selection + arrow visual state
        if current_tab == TAB_HINGE:
            if pygame.K_LEFT in keys_down:
                node.selected_hinge = "left"
            elif pygame.K_RIGHT in keys_down:
                node.selected_hinge = "right"

            node.hinge_arrows["up"]   = pygame.K_UP in keys_down
            node.hinge_arrows["down"] = pygame.K_DOWN in keys_down

        # NAVIGATION TAB: nav_arrows controlled by arrow keys, waves also from IR
        else:
            node.nav_arrows["up"]    = pygame.K_UP in keys_down
            node.nav_arrows["down"]  = pygame.K_DOWN in keys_down
            node.nav_arrows["left"]  = pygame.K_LEFT in keys_down
            node.nav_arrows["right"] = pygame.K_RIGHT in keys_down

            # Optional: keyboard toggles on top of IR sensor input
            if pygame.K_1 in keys_down:
                node.waves_state["top"] = not node.waves_state["top"]
                keys_down.remove(pygame.K_1)
            if pygame.K_2 in keys_down:
                node.waves_state["right_top"] = not node.waves_state["right_top"]
                keys_down.remove(pygame.K_2)
            if pygame.K_3 in keys_down:
                node.waves_state["right_bottom"] = not node.waves_state["right_bottom"]
                keys_down.remove(pygame.K_3)
            if pygame.K_4 in keys_down:
                node.waves_state["bottom"] = not node.waves_state["bottom"]
                keys_down.remove(pygame.K_4)
            if pygame.K_5 in keys_down:
                node.waves_state["left_bottom"] = not node.waves_state["left_bottom"]
                keys_down.remove(pygame.K_5)
            if pygame.K_6 in keys_down:
                node.waves_state["left_top"] = not node.waves_state["left_top"]
                keys_down.remove(pygame.K_6)

        # ---- Publish drive/turn from nav arrows ----
        node.publish_drive_and_turn(current_tab)
        node.publish_hinge_drive(current_tab)
        
        # --- Update current angles towards target angles ---
        smooth_speed = 2.5
        
        def approach(current, target, speed):
            if current < target:
                return min(current + speed, target)
            elif current > target:
                return max(current - speed, target)
            return current

        node.current_angle_left = approach(node.current_angle_left, node.target_angle_left, smooth_speed)
        node.current_angle_right = approach(node.current_angle_right, node.target_angle_right, smooth_speed)

        # ---- Draw ----
        screen.fill(BG)

        if current_tab == TAB_HINGE:
            draw_hinge_view(screen, node)
        else:
            draw_navigation_view(screen, node)

        draw_tabs(screen, current_tab)

        pygame.display.flip()
        clock.tick(60)

    node.destroy_node()
    rclpy.shutdown()
    pygame.quit()


if __name__ == "__main__":
    main()
