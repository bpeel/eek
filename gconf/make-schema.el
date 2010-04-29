;;
;; eek - An emulator for the Acorn Electron
;; Copyright (C) 2010  Neil Roberts
;;
;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

(require 'xml)

(defun eek-attrib-node (name &rest attribs)
  (let (children)
    (while attribs
      (push (cons (car attribs) (cons () (cons (cadr attribs) nil)))
	    children)
      (setq attribs (cddr attribs)))
    (cons name (cons () (nreverse children)))))

(defun eek-rom-schema (key short-desc long-desc)
  (nconc (eek-attrib-node 'schema
			  'key (concat "/schemas/apps/eek/roms/" key)
			  'applyto (concat "/apps/eek/roms/" key)
			  'owner "eek"
			  'type "string")
	 (list (let ((locale (eek-attrib-node 'locale
					      'default ""
					      'short short-desc
					      'long long-desc)))
		 (push (cons 'name "C") (nth 1 locale))
		 locale))))

(defun eek-fast-rom-schema (num)
  (eek-rom-schema (concat "rom_" (number-to-string num))
		  (concat "Filename of ROM in slot "
			  (number-to-string num))
		  (concat "Filename of ROM that is used when page number "
			  (number-to-string num) " is selected. "
			  "This slot is a fast ROM because it only "
			  "requires one write to the paging register "
			  "to select.")))

(defun eek-slow-rom-schema (num)
  (eek-rom-schema (concat "rom_" (number-to-string num))
		  (concat "Filename of ROM in slot "
			  (number-to-string num))
		  (concat "Filename of ROM that is used when page number "
			  (number-to-string num) " is selected. "
			  "This slot is a slow ROM because it "
			  "requires two writes to the paging register "
			  "to select. The first write causes the BASIC "
			  "ROM to be deselected.")))

(defun eek-basic-rom-schema ()
  (eek-rom-schema "basic_rom"
		  "Filename of the BASIC language ROM."
		  (concat "Filename of the ROM that is used when page number "
			  "10 or 11 is selected. This is usually where the "
			  "BASIC language ROM is found.")))

(defun eek-os-rom-schema ()
  (eek-rom-schema "os_rom"
		  "Filename of the OS ROM."
		  (concat "Filename of the ROM that occupies "
			  "memory locations 0xC000 to 0xFFFF. "
			  "This is usually where the operating system "
			  "is found.")))

(defun eek-schemalist ()
  (list 'gconfschemafile ()
	(cons 'schemalist
	      (cons () (append (list (eek-os-rom-schema)
				     (eek-basic-rom-schema))
			       (mapcar 'eek-fast-rom-schema
				       '(12 13 14 15))
			       (mapcar 'eek-slow-rom-schema
				       '(0 1 2 3 4 5 6 7)))))))

(defun eek-print-schemalist ()
  (interactive)
  (xml-print (list (eek-schemalist))))
