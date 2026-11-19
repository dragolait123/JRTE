using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace JRTE.App;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new JRTEWindow());
    }
}

internal sealed class JRTEWindow : Form
{
    private readonly VideoSurface _video = new();
    private readonly Label _status = new();
    private readonly Label _fileName = new();
    private readonly Label _filePath = new();
    private readonly Label _fileSize = new();
    private readonly Label _engineState = new();
    private readonly Label _navState = new();
    private readonly ToolTip _tips = new();

    private Button? _activeNav;
    private string? _selectedFile;

    private readonly Color background = Color.FromArgb(10, 15, 24);
    private readonly Color sidebarColor = Color.FromArgb(15, 22, 34);
    private readonly Color cardColor = Color.FromArgb(19, 28, 42);
    private readonly Color accent = Color.FromArgb(72, 145, 255);
    private readonly Color muted = Color.FromArgb(143, 159, 181);
    private readonly Color foreground = Color.FromArgb(233, 240, 250);

    public JRTEWindow()
    {
        Text = "JRTE Desktop";
        StartPosition = FormStartPosition.CenterScreen;
        Size = new Size(1440, 900);
        MinimumSize = new Size(1100, 700);
        BackColor = background;
        ForeColor = foreground;
        Font = new Font("Segoe UI", 9F);
        DoubleBuffered = true;

        BuildInterface();
        SetStatus("JRTE Desktop initialisé. En attente d'une vidéo.");
    }

    private void BuildInterface()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = background,
            Margin = Padding.Empty
        };

        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 238));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        Controls.Add(root);

        root.Controls.Add(BuildSidebar(), 0, 0);
        root.Controls.Add(BuildWorkspace(), 1, 0);
    }

    private Control BuildSidebar()
    {
        var sidebar = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = sidebarColor,
            Padding = new Padding(18, 24, 18, 18)
        };

        var brand = new Label
        {
            Text = "JRTE",
            Font = new Font("Segoe UI", 25, FontStyle.Bold),
            ForeColor = accent,
            Dock = DockStyle.Top,
            Height = 42
        };

        var subtitle = new Label
        {
            Text = "NATIVE VIDEO PLAYER",
            Font = new Font("Segoe UI", 8, FontStyle.Bold),
            ForeColor = muted,
            Dock = DockStyle.Top,
            Height = 28
        };

        var line = new Panel
        {
            Dock = DockStyle.Top,
            Height = 1,
            BackColor = Color.FromArgb(43, 57, 76),
            Margin = new Padding(0, 12, 0, 18)
        };

        var menu = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            Height = 210,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            BackColor = sidebarColor,
            Margin = Padding.Empty
        };

        AddNav(menu, "  Vue d'ensemble", true);
        AddNav(menu, "  Bibliothèque", false);
        AddNav(menu, "  Diagnostics", false);

        var version = new Label
        {
            Text = "JRTE DESKTOP\nVersion de développement\nMoteur natif C++",
            ForeColor = muted,
            Font = new Font("Segoe UI", 8.5F),
            Dock = DockStyle.Bottom,
            Height = 72
        };

        sidebar.Controls.Add(version);
        sidebar.Controls.Add(menu);
        sidebar.Controls.Add(line);
        sidebar.Controls.Add(subtitle);
        sidebar.Controls.Add(brand);

        return sidebar;
    }

    private void AddNav(FlowLayoutPanel menu, string text, bool selected)
    {
        var button = new Button
        {
            Text = text,
            Width = 198,
            Height = 43,
            TextAlign = ContentAlignment.MiddleLeft,
            FlatStyle = FlatStyle.Flat,
            BackColor = selected ? Color.FromArgb(33, 55, 84) : sidebarColor,
            ForeColor = selected ? Color.White : muted,
            Font = new Font("Segoe UI", 9.5F, FontStyle.Bold),
            Margin = new Padding(0, 4, 0, 5),
            Cursor = Cursors.Hand
        };

        button.FlatAppearance.BorderSize = 0;
        button.FlatAppearance.MouseOverBackColor = Color.FromArgb(30, 43, 62);

        if (selected)
            _activeNav = button;

        button.Click += (_, _) =>
        {
            if (_activeNav != null)
            {
                _activeNav.BackColor = sidebarColor;
                _activeNav.ForeColor = muted;
            }

            _activeNav = button;
            button.BackColor = Color.FromArgb(33, 55, 84);
            button.ForeColor = Color.White;

            string section = text.Trim();
            _navState.Text = section.ToUpperInvariant();

            SetStatus(section + " : interface en cours de développement.");
        };

        menu.Controls.Add(button);
    }

    private Control BuildWorkspace()
    {
        var workspace = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            BackColor = background,
            Padding = Padding.Empty
        };

        workspace.RowStyles.Add(new RowStyle(SizeType.Absolute, 82));
        workspace.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        workspace.Controls.Add(BuildTopbar(), 0, 0);
        workspace.Controls.Add(BuildDashboard(), 0, 1);

        return workspace;
    }

    private Control BuildTopbar()
    {
        var top = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = background,
            Padding = new Padding(26, 14, 26, 10)
        };

        var title = new Label
        {
            Text = "Tableau de bord",
            Font = new Font("Segoe UI", 19, FontStyle.Bold),
            ForeColor = foreground,
            AutoSize = true,
            Location = new Point(24, 12)
        };

        var description = new Label
        {
            Text = "Centre de contrôle du lecteur JRTE",
            Font = new Font("Segoe UI", 9),
            ForeColor = muted,
            AutoSize = true,
            Location = new Point(27, 45)
        };

        var badge = new Label
        {
            Text = "●  MOTEUR EN DÉVELOPPEMENT",
            AutoSize = true,
            ForeColor = Color.FromArgb(255, 194, 92),
            BackColor = cardColor,
            Font = new Font("Segoe UI", 8, FontStyle.Bold),
            Padding = new Padding(12, 9, 12, 9),
            Anchor = AnchorStyles.Top | AnchorStyles.Right
        };

        badge.Location = new Point(Width - 440, 20);
        top.Resize += (_, _) =>
            badge.Location = new Point(top.ClientSize.Width - badge.Width - 25, 20);

        top.Controls.Add(title);
        top.Controls.Add(description);
        top.Controls.Add(badge);

        return top;
    }

    private Control BuildDashboard()
    {
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 2,
            Padding = new Padding(18, 8, 18, 18),
            BackColor = background
        };

        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 68));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 32));
        grid.RowStyles.Add(new RowStyle(SizeType.Percent, 66));
        grid.RowStyles.Add(new RowStyle(SizeType.Percent, 34));

        grid.Controls.Add(BuildPreviewCard(), 0, 0);
        grid.Controls.Add(BuildFileCard(), 1, 0);
        grid.Controls.Add(BuildControlsCard(), 0, 1);
        grid.Controls.Add(BuildDiagnosticsCard(), 1, 1);

        return grid;
    }

    private Panel CreateCard(string title)
    {
        var card = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = cardColor,
            Padding = new Padding(18),
            Margin = new Padding(7)
        };

        var heading = new Label
        {
            Text = title,
            ForeColor = foreground,
            Font = new Font("Segoe UI", 10, FontStyle.Bold),
            Dock = DockStyle.Top,
            Height = 34
        };

        card.Controls.Add(heading);
        return card;
    }

    private Control BuildPreviewCard()
    {
        var card = CreateCard("APERÇU VIDÉO");

        _video.Dock = DockStyle.Fill;
        _video.Margin = new Padding(0, 5, 0, 0);
        card.Controls.Add(_video);

        return card;
    }

    private Control BuildFileCard()
    {
        var card = CreateCard("INFORMATIONS DU FICHIER");

        var details = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoScroll = true,
            BackColor = cardColor,
            Padding = new Padding(0, 10, 0, 0)
        };

        AddInfo(details, "FICHIER SÉLECTIONNÉ");
        _fileName.Text = "Aucune vidéo";
        AddValue(details, _fileName);

        AddInfo(details, "EMPLACEMENT");
        _filePath.Text = "En attente d'un fichier";
        AddValue(details, _filePath);

        AddInfo(details, "TAILLE");
        _fileSize.Text = "—";
        AddValue(details, _fileSize);

        AddInfo(details, "ÉTAT DU MOTEUR");
        _engineState.Text = "Initialisé — décodage en développement";
        AddValue(details, _engineState);

        card.Controls.Add(details);
        return card;
    }

    private void AddInfo(FlowLayoutPanel parent, string text)
    {
        parent.Controls.Add(new Label
        {
            Text = text,
            Width = 330,
            Height = 24,
            ForeColor = muted,
            Font = new Font("Segoe UI", 8, FontStyle.Bold),
            Margin = new Padding(0, 9, 0, 0)
        });
    }

    private void AddValue(FlowLayoutPanel parent, Label label)
    {
        label.Width = 330;
        label.Height = 42;
        label.ForeColor = foreground;
        label.Font = new Font("Segoe UI", 9.5F);
        label.AutoEllipsis = true;
        label.Margin = new Padding(0, 0, 0, 4);
        parent.Controls.Add(label);
    }

    private Control BuildControlsCard()
    {
        var card = CreateCard("COMMANDES DE LECTURE");

        var controls = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            Height = 62,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = true,
            BackColor = cardColor,
            Padding = new Padding(0, 8, 0, 0)
        };

        var open = MakeButton("Ouvrir une vidéo", accent, Color.White, 155);
        open.Click += (_, _) => OpenVideo();

        var play = MakeButton("Lecture", Color.FromArgb(36, 53, 73), foreground, 105);
        play.Enabled = false;
        _tips.SetToolTip(play, "La lecture sera activée lorsque le décodeur H.264 sera intégré.");

        var stop = MakeButton("Arrêt", Color.FromArgb(36, 53, 73), foreground, 90);
        stop.Click += (_, _) => SetStatus("Arrêt demandé. Aucun flux vidéo actif.");

        controls.Controls.Add(open);
        controls.Controls.Add(play);
        controls.Controls.Add(stop);

        var timeline = new ProgressBar
        {
            Dock = DockStyle.Top,
            Height = 10,
            Style = ProgressBarStyle.Continuous,
            Value = 0,
            Margin = new Padding(0, 12, 0, 0)
        };

        var timelineText = new Label
        {
            Text = "00:00     —     Durée inconnue",
            ForeColor = muted,
            Dock = DockStyle.Top,
            Height = 28,
            TextAlign = ContentAlignment.MiddleLeft
        };

        card.Controls.Add(timelineText);
        card.Controls.Add(timeline);
        card.Controls.Add(controls);

        return card;
    }

    private Control BuildDiagnosticsCard()
    {
        var card = CreateCard("ÉTAT DU SYSTÈME");

        var text = new Label
        {
            Dock = DockStyle.Fill,
            ForeColor = muted,
            Font = new Font("Segoe UI", 9),
            Text = "●  Interface WinForms : active\r\n\r\n" +
                   "●  Rendu BGRA : disponible\r\n\r\n" +
                   "●  Moteur natif C++ : développement\r\n\r\n" +
                   "○  Décodage vidéo complet : à intégrer",
            Padding = new Padding(0, 12, 0, 0)
        };

        card.Controls.Add(text);
        return card;
    }

    private Button MakeButton(string text, Color back, Color fore, int width)
    {
        var button = new Button
        {
            Text = text,
            Width = width,
            Height = 39,
            BackColor = back,
            ForeColor = fore,
            FlatStyle = FlatStyle.Flat,
            Font = new Font("Segoe UI", 9, FontStyle.Bold),
            Cursor = Cursors.Hand,
            Margin = new Padding(0, 0, 9, 8)
        };

        button.FlatAppearance.BorderSize = 0;
        button.FlatAppearance.MouseOverBackColor = Color.FromArgb(65, 91, 126);

        return button;
    }

    private void OpenVideo()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Sélectionner une vidéo pour JRTE",
            Filter = "Vidéos|*.mp4;*.h264;*.264;*.m4v|Tous les fichiers|*.*",
            CheckFileExists = true,
            Multiselect = false
        };

        if (dialog.ShowDialog(this) != DialogResult.OK)
            return;

        var info = new FileInfo(dialog.FileName);
        _selectedFile = info.FullName;

        _fileName.Text = info.Name;
        _filePath.Text = info.FullName;
        _fileSize.Text = $"{info.Length / 1024.0 / 1024.0:F2} MB";

        SetStatus("Fichier sélectionné. L'analyse du conteneur sera connectée au moteur natif.");
    }

    private void SetStatus(string message)
    {
        _status.Text = "  ●  " + message;
        _status.ForeColor = muted;
    }
}

public sealed class VideoSurface : Control
{
    private Bitmap? _frame;
    private readonly object _frameLock = new();

    public VideoSurface()
    {
        BackColor = Color.FromArgb(6, 10, 16);
        SetStyle(
            ControlStyles.UserPaint |
            ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.ResizeRedraw,
            true
        );
    }

    public void PresentFrame(byte[] bgra, int width, int height)
    {
        ArgumentNullException.ThrowIfNull(bgra);

        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width));

        int expectedSize = checked(width * height * 4);

        if (bgra.Length != expectedSize)
            throw new ArgumentException("Taille BGRA incorrecte.");

        if (InvokeRequired)
        {
            byte[] copy = (byte[])bgra.Clone();

            if (!IsDisposed && IsHandleCreated)
                BeginInvoke(new Action(() => PresentFrame(copy, width, height)));

            return;
        }

        Bitmap next = new(width, height, PixelFormat.Format32bppArgb);

        BitmapData data = next.LockBits(
            new Rectangle(0, 0, width, height),
            ImageLockMode.WriteOnly,
            PixelFormat.Format32bppArgb
        );

        try
        {
            for (int y = 0; y < height; y++)
            {
                Marshal.Copy(
                    bgra,
                    y * width * 4,
                    IntPtr.Add(data.Scan0, y * data.Stride),
                    width * 4
                );
            }
        }
        finally
        {
            next.UnlockBits(data);
        }

        lock (_frameLock)
        {
            Bitmap? previous = _frame;
            _frame = next;
            previous?.Dispose();
        }

        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.Clear(BackColor);

        lock (_frameLock)
        {
            if (_frame == null)
            {
                string message =
                    "JRTE VIDEO OUTPUT\r\n\r\n" +
                    "Aucune image décodée\r\n" +
                    "Sélectionne un fichier vidéo pour commencer.";

                using var font = new Font("Segoe UI", 12, FontStyle.Bold);
                using var smallFont = new Font("Segoe UI", 9);
                using var brush = new SolidBrush(Color.FromArgb(150, 169, 194));

                var titleSize = e.Graphics.MeasureString("JRTE VIDEO OUTPUT", font);
                e.Graphics.DrawString(
                    "JRTE VIDEO OUTPUT",
                    font,
                    brush,
                    (ClientSize.Width - titleSize.Width) / 2,
                    ClientSize.Height / 2 - 45
                );

                var subSize = e.Graphics.MeasureString(
                    "Aucune image décodée",
                    smallFont
                );

                e.Graphics.DrawString(
                    "Aucune image décodée",
                    smallFont,
                    brush,
                    (ClientSize.Width - subSize.Width) / 2,
                    ClientSize.Height / 2 + 2
                );

                return;
            }

            float scale = Math.Min(
                (float)ClientSize.Width / _frame.Width,
                (float)ClientSize.Height / _frame.Height
            );

            int drawWidth = (int)(_frame.Width * scale);
            int drawHeight = (int)(_frame.Height * scale);

            int x = (ClientSize.Width - drawWidth) / 2;
            int y = (ClientSize.Height - drawHeight) / 2;

            e.Graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            e.Graphics.DrawImage(_frame, new Rectangle(x, y, drawWidth, drawHeight));
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            lock (_frameLock)
            {
                _frame?.Dispose();
                _frame = null;
            }
        }

        base.Dispose(disposing);
    }
}