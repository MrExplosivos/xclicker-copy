#include <gtk/gtk.h>
#include "xclicker-app.h"
#include "mainwin.h"
#include "x11api.h"
#include "settings.h"
#include "macros.h"

struct _MainAppWindow
{
	GtkApplicationWindow parent;
	GtkWidget *pwin;

	// Entries
	GtkWidget *hours_entry;
	GtkWidget *minutes_entry;
	GtkWidget *seconds_entry;
	GtkWidget *millisecs_entry;
	GtkWidget *repeat_entry;
	GtkWidget *mouse_entry;
	GtkWidget *x_entry;
	GtkWidget *y_entry;

	// Checkboxes
	GtkWidget *repeat_only_check;
	GtkWidget *custom_location_check;

	// Bottom buttons
	GtkWidget *start_button;
	GtkWidget *stop_button;
	GtkWidget *settings_button;
	GtkWidget *set_button;
} mainappwindow;
G_DEFINE_TYPE(MainAppWindow, main_app_window, GTK_TYPE_APPLICATION_WINDOW);


gboolean isClicking = FALSE;
gboolean isChoosingLocation = FALSE;
//gboolean beforeWindowTopmost = FALSE; // For later.

struct _click_opts
{
	int sleep;
	int button;

	gboolean repeat;
	int repeat_times;

	gboolean custom_location;
	int custom_x, custom_y;
} click_opts;

void toggle_buttons()
{
	gtk_widget_set_sensitive(GTK_WIDGET(mainappwindow.start_button), !isClicking);
	gtk_widget_set_sensitive(GTK_WIDGET(mainappwindow.stop_button), isClicking);
}

void click_handler()
{
	Display *display = get_display();
	int count = 0;
	while (isClicking)
	{
		if (click_opts.custom_location)
			move_to(display, click_opts.custom_x, click_opts.custom_y);
		click(display, click_opts.button);
		usleep(click_opts.sleep * 1000); // Milliseconds
		if (click_opts.repeat)
		{
			if (count >= click_opts.repeat_times)
				isClicking = FALSE;
			else
				count++;
		}
	}
	// Free?
	XCloseDisplay(display);
	g_idle_add(toggle_buttons, NULL);
}

struct _set_coord_args
{
	const char *coordx;
	const char *coordy;
} set_coord_args;

void set_coords()
{
	if (GTK_IS_ENTRY(mainappwindow.x_entry))
		gtk_entry_set_text(mainappwindow.x_entry, set_coord_args.coordx);
	if (GTK_IS_ENTRY(mainappwindow.y_entry))
		gtk_entry_set_text(mainappwindow.y_entry, set_coord_args.coordy);
}

void get_cursor_pos_click_handler()
{
	Display *display = get_display();
	init_mouse_config(display);

	while (isChoosingLocation)
	{
		if (get_mousebutton_state(display) == 1)
			isChoosingLocation = FALSE;
	}
}

// Toggle window settings if choosing location or finished.
void toggle_window_from_set()
{
	switch (isChoosingLocation)
	{
	case TRUE:
		gtk_window_set_keep_above(GTK_WINDOW(mainappwindow.pwin), TRUE);
		gtk_button_set_label(GTK_BUTTON(mainappwindow.set_button), "Click");
		gtk_widget_set_sensitive(mainappwindow.set_button, FALSE);
		break;
	case FALSE:
		gtk_window_set_keep_above(GTK_WINDOW(mainappwindow.pwin), FALSE);
		gtk_button_set_label(GTK_BUTTON(mainappwindow.set_button), "Set");
		gtk_widget_set_sensitive(mainappwindow.set_button, TRUE);
		break;
	}
}

void get_cursor_pos_handler()
{
	Display *display = get_display();
	char *cur_x, *cur_y;
	while (isChoosingLocation)
	{
		int i_cur_x, i_cur_y;
		get_mouse_coords(display, &i_cur_x, &i_cur_y);
		cur_x = (char *)malloc(sizeof(i_cur_x));
		cur_y = (char *)malloc(sizeof(i_cur_y));
		sprintf(cur_x, "%d", i_cur_x);
		sprintf(cur_y, "%d", i_cur_y);

		// TODO: Implement a way of doing this with user_data instead of this.
		set_coord_args.coordx = cur_x;
		set_coord_args.coordy = cur_y;
		g_main_context_invoke(NULL, set_coords, NULL);

		// Old, no thread.
		// if (strcmp(gtk_entry_get_text(GTK_ENTRY(mainappwindow.x_entry)), cur_x) != 0)
		// 	gtk_entry_set_text(GTK_ENTRY(mainappwindow.x_entry), cur_x);
		// if (strcmp(gtk_entry_get_text(GTK_ENTRY(mainappwindow.y_entry)), cur_y) != 0)
		// 	gtk_entry_set_text(GTK_ENTRY(mainappwindow.y_entry), cur_y);
		usleep(50000);
	}
	if (cur_x != NULL)
		free(cur_x);
	if (cur_y != NULL)
		free(cur_y);
	g_idle_add(toggle_window_from_set, NULL);
}



int get_text_to_int(GtkWidget *entry)
{
	return atoi(gtk_entry_get_text(GTK_ENTRY(entry)));
}

// Handlers
void repeat_only_check_toggle(GtkToggleButton *check)
{
	gtk_widget_set_sensitive(mainappwindow.repeat_entry, gtk_toggle_button_get_active(check));
}

void custom_location_check_toggle(GtkToggleButton *check)
{
	gboolean active = gtk_toggle_button_get_active(check);
	gtk_widget_set_sensitive(mainappwindow.x_entry, active);
	gtk_widget_set_sensitive(mainappwindow.y_entry, active);
	gtk_widget_set_sensitive(mainappwindow.set_button, active);
}

// gint length, gint *position, gpointer user_data
void insert_handler(GtkEditable *editable, const gchar *text)
{
	if (!g_unichar_isdigit(g_utf8_get_char(text)))
	{
		g_signal_stop_emission_by_name(editable, "insert_text");
	}
}

//GtkButton *button
void start_clicked()
{
	int sleep = get_text_to_int(mainappwindow.hours_entry) * 3600000 + get_text_to_int(mainappwindow.minutes_entry) * 60000 + get_text_to_int(mainappwindow.seconds_entry) * 1000 + get_text_to_int(mainappwindow.millisecs_entry);

	if (sleep < 10 && is_safemode())
	{
		GtkDialog *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Warning");
		gtk_message_dialog_format_secondary_text(dialog, "Intervals under 10 milliseconds is restricted because of safe mode.");
		gtk_dialog_run(dialog);
		gtk_widget_destroy(dialog);
		return;
	}

	isClicking = TRUE;
	toggle_buttons();

	//click_opts.button = Button1;
	click_opts.sleep = sleep;
	const gchar *selectedtext = gtk_entry_get_text(GTK_ENTRY(mainappwindow.mouse_entry));
	if (strcmp(selectedtext, "Right") == 0)
		click_opts.button = Button3;
	else if (strcmp(selectedtext, "Middle") == 0)
		click_opts.button = Button2;
	else
		click_opts.button = Button1;

	if ((click_opts.repeat = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mainappwindow.repeat_only_check))))
		click_opts.repeat_times = get_text_to_int(mainappwindow.repeat_entry);

	if ((click_opts.custom_location = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mainappwindow.custom_location_check))))
	{
		click_opts.custom_x = get_text_to_int(mainappwindow.x_entry);
		click_opts.custom_y = get_text_to_int(mainappwindow.y_entry);
	}
	g_thread_new("click_handler", click_handler, NULL);
}

void stop_clicked()
{
	isClicking = FALSE;
	toggle_buttons();
}

void settings_clicked()
{
	settings_dialog_new();
}

void set_button_clicked()
{
	isChoosingLocation = TRUE;
	toggle_window_from_set();
	g_thread_new("get_cursor_pos_click_handler", get_cursor_pos_click_handler, NULL);
	g_thread_new("get_cursor_pos_handler", get_cursor_pos_handler, NULL);
}

static void main_app_window_init(MainAppWindow *win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
	config_init();

	mainappwindow.pwin = gtk_widget_get_toplevel(win);

	// Entries
	mainappwindow.hours_entry = win->hours_entry;
	mainappwindow.minutes_entry = win->minutes_entry;
	mainappwindow.seconds_entry = win->seconds_entry;
	mainappwindow.millisecs_entry = win->millisecs_entry;
	mainappwindow.repeat_entry = win->repeat_entry;
	mainappwindow.mouse_entry = win->mouse_entry;
	mainappwindow.x_entry = win->x_entry;
	mainappwindow.y_entry = win->y_entry;

	// Checkboxes
	mainappwindow.repeat_only_check = win->repeat_only_check;
	mainappwindow.custom_location_check = win->custom_location_check;

	// Buttons
	mainappwindow.start_button = win->start_button;
	mainappwindow.stop_button = win->stop_button;
	mainappwindow.settings_button = win->settings_button;
	mainappwindow.set_button = win->set_button;
}

static void main_app_window_class_init(MainAppWindowClass *class)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/org/gtk/xclicker/ui/xclicker-window.ui");
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), insert_handler);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), start_clicked);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), stop_clicked);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), repeat_only_check_toggle);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), settings_clicked);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), custom_location_check_toggle);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), set_button_clicked);

	// Entries
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, hours_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, minutes_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, seconds_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, millisecs_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, repeat_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, mouse_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, x_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, y_entry);

	// Checkboxes
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, repeat_only_check);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, custom_location_check);

	// Buttons
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, start_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, stop_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, settings_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), MainAppWindow, set_button);
}

MainAppWindow *main_app_window_new(XClickerApp *app)
{
	return g_object_new(MAIN_APP_WINDOW_TYPE, "application", app, NULL);
}

void main_app_window_open(MainAppWindow *UNUSED(win), GFile *UNUSED(file))
{
}