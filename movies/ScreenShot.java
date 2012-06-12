import java.awt.Canvas;
import java.awt.Frame;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Toolkit;
import java.awt.Window;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.io.File;


public class ScreenShot {
	public static void main(String[] args) {
		Window window = new Window(new Frame()); 
		SCanvas canvas = new SCanvas(args[0]);
		window.add(canvas);
		window.setBounds(100, 100, 450, 338);

		window.setVisible(true);
		window.toFront();
	}
	
	static class SCanvas extends Canvas implements Runnable, MouseListener {
		private static final long serialVersionUID = 1L;
		Toolkit toolkit;
		Graphics og;
		Image bg;
		Image sc1;
		Image sc2;
		Image toast1;
		Image toast2;
		Image toast3;
		Image ul;
		
		Image mainSc;
		Image toast;
		int off = 338 + 2;
		
		SCanvas(String baseDir) {
			toolkit = Toolkit.getDefaultToolkit();
			sc1 = toolkit.createImage(baseDir + File.separator + "000.png");
			sc2 = toolkit.createImage(baseDir + File.separator + "400.png");
			toast1 = toolkit.createImage(baseDir + File.separator + "toast1.png");
			toast2 = toolkit.createImage(baseDir + File.separator + "toast2.png");
			toast3 = toolkit.createImage(baseDir + File.separator + "toast3.png");
			ul = toolkit.createImage(baseDir + File.separator + "underline.png");
			addMouseListener(this);
		}
		
		public void run() {
			mainSc = sc1;
			toast = toast1;
			boolean up = true;
			try {
				Thread.sleep(3000);
				for (int i = 0; ;i++) {
					Thread.sleep(10);
					if (up) {
						if (off != 338 - 140) {
							off--;
						} else {
							if (toast == toast3) {
								synchronized (this) {
									wait();
								}
								for (int j = 0; j < 5; j++) {
									repaint();
									Thread.sleep(10);
									off--;
								}
								mainSc = sc2;
							} else {
								Thread.sleep(5000);
							}
							up = false;
						}
					} else {
						if (off != 338 + 2) {
							off++;
						} else {
							if (toast == toast3) {
								break;
							}
							Thread.sleep(300);
							up = true;
							if (toast == toast1) {
								toast = toast2;
							} else if (toast == toast2) {
								toast = toast3;
							}
						}
					}
					repaint();
				}
			} catch (InterruptedException e) {
			}
		}
		
		public void paint(Graphics g) {
			if (bg == null) {
				bg = createImage(450, 338);
				og = bg.getGraphics();
				og.drawImage(sc1, 0, 0, this);
				og.drawImage(sc2, 0, 0, this);
				og.drawImage(toast1, 0, 0, this);
				og.drawImage(toast2, 0, 0, this);
				og.drawImage(toast3, 0, 0, this);
				og.drawImage(ul, 0, 0, this);
				new Thread(this).start();
			} else {
				og.clearRect(0, 0, 450, 338);
			}
			
			og.drawImage(mainSc, 0, 0, this);
			og.drawImage(toast, 50, off, this);
			og.drawImage(ul, 0, 335, this);
			
			g.drawImage(bg, 0, 0, this);
		}
		public void update(Graphics g) {
			paint(g);
		}

		@Override
		public void mouseClicked(MouseEvent e) {
			if (toast == toast3) {
				synchronized (this) {
					notify();
				}
			}
		}

		@Override
		public void mouseEntered(MouseEvent e) {
		}

		@Override
		public void mouseExited(MouseEvent e) {
		}

		@Override
		public void mousePressed(MouseEvent e) {
		}

		@Override
		public void mouseReleased(MouseEvent e) {
		}
	}
}

